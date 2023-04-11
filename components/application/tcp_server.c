/**
 *******************************************************************************
 * @file    tcp_server.c
 * @author  Dmytro Shevchenko
 * @brief   TCP server for receive data from client source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "tcp_server.h"

#include "app_events.h"
#include "app_timers.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "tcp_transport.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[CMD Srv] "
#define DEBUG_LVL   PRINT_WARNING

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define PAYLOAD_SIZE                   256
#define CONFIG_TCPIP_EVENT_THD_WA_SIZE 4096

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                                      \
  STATE( DISABLED, _disabled_state_handler_array )               \
  STATE( IDLE, _idle_state_handler_array )                       \
  STATE( WAIT_CONNECTION, _wait_connection_state_handler_array ) \
  STATE( WORKING, _working_state_handler_array )

/* Private types -------------------------------------------------------------*/

typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} state_t;

// typedef enum
// {
//   TIMER_ID_CONNECT_TIMEOUT,
//   TIMER_ID_UPDATE_WIFI_INFO,
//   TIMER_ID_LAST
// } timer_id;

typedef struct
{
  state_t state;
  int server_socket;
  int client_socket;
  bool ethernet_is_connected;
  char payload[PAYLOAD_SIZE];
  QueueHandle_t queue;
  //   keepAlive_t keepAlive;
} module_context_t;

/* Private variables ---------------------------------------------------------*/

static module_context_t ctx;

/* Private functions declaration ---------------------------------------------*/

// static void _timeout_connect_cb( TimerHandle_t xTimer );
// static void _update_info_cb( TimerHandle_t xTimer );

static void _state_common_event_deinit_request( const app_event_t* event );
static void _state_common_event_ethernet_connected( const app_event_t* event );
static void _state_common_event_ethernet_disconnected( const app_event_t* event );
static void _state_common_event_close_socket( const app_event_t* event );

static void _state_disabled_event_init_request( const app_event_t* event );

static void _state_idle_event_prepare_socket( const app_event_t* event );

static void _state_wait_connecting_event_wait_connection( const app_event_t* event );

static void _state_working_event_wait_client_data( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_TCP_SERVER_PREPARE_SOCKET, _state_idle_event_prepare_socket ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_CLOSE_SOCKET, _state_common_event_close_socket ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_ETHERNET_CONNECTED, _state_common_event_ethernet_connected ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_ETHERNET_DISCONNECTED, _state_common_event_ethernet_disconnected ),
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
};

static const struct app_events_handler _wait_connection_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_TCP_SERVER_WAIT_CONNECTION, _state_wait_connecting_event_wait_connection ),
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_CLOSE_SOCKET, _state_common_event_close_socket ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_ETHERNET_DISCONNECTED, _state_common_event_ethernet_disconnected ),
};

static const struct app_events_handler _working_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_TCP_SERVER_WAIT_CLIENT_DATA, _state_working_event_wait_client_data ),
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_CLOSE_SOCKET, _state_common_event_close_socket ),
    EVENT_ITEM( MSG_ID_TCP_SERVER_ETHERNET_DISCONNECTED, _state_common_event_ethernet_disconnected ),
};

struct state_context
{
  const state_t state;
  const char* name;
  const struct app_events_handler* event_handler_array;
  uint8_t event_handler_array_size;
};

static const struct state_context module_state[STATE_TOP] =
  {
#define STATE( _state, _event_handler_array )    \
  {                                              \
    .state = _state,                             \
    .name = #_state,                             \
    .event_handler_array = _event_handler_array, \
    .event_handler_array_size = ARRAY_SIZE( _event_handler_array ) },
    STATE_HANDLER_ARRAY
#undef STATE
};

// static app_timer_t timers[] =
//   {
//     TIMER_ITEM( TIMER_ID_CONNECT_TIMEOUT, _timeout_connect_cb, 1500, "WiFiConnect" ),
//     TIMER_ITEM( TIMER_ID_UPDATE_WIFI_INFO, _update_info_cb, 1000, "WiFiUpdate" ) };

/* Private functions ---------------------------------------------------------*/

static const char* _get_state_name( const state_t state )
{
  return module_state[state].name;
}

static void _change_state( state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_TCP_SERVER, APP_EVENT_TCP_SERVER );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_TCP_SERVER, APP_EVENT_TCP_SERVER, data, data_size );
  }

  TCPServer_PostMsg( &event );
}

/* Sate machine functions ---------------------------------------------------*/

static void _state_common_event_deinit_request( const app_event_t* event )
{
  _state_common_event_close_socket( NULL );
  _change_state( DISABLED );
}

static void _state_common_event_ethernet_connected( const app_event_t* event )
{
  ctx.ethernet_is_connected = true;
  _send_internal_event( MSG_ID_TCP_SERVER_PREPARE_SOCKET, NULL, 0 );
}

static void _state_common_event_ethernet_disconnected( const app_event_t* event )
{
  ctx.ethernet_is_connected = false;
  _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
}

static void _state_common_event_close_socket( const app_event_t* event )
{
  if ( ctx.client_socket != -1 )
  {
    TCPTransport_Close( ctx.client_socket );
    ctx.client_socket = -1;
  }

  osDelay( 50 );
  if ( ctx.server_socket != -1 )
  {
    TCPTransport_Close( ctx.server_socket );
    ctx.server_socket = -1;
  }

  //keepAliveStop(&ctx.keepAlive);
  if ( ctx.state == WORKING )
  {
    bool result = false;
    app_event_t response = { 0 };
    AppEventPrepareWithData( &response, MSG_ID_NETWORK_MANAGER_TCP_SERVER_CLIENT_STATUS, APP_EVENT_TCP_SERVER, APP_EVENT_NETWORK_MANAGER, &result, sizeof( result ) );
    NetworkManagerPostMsg( &response );
  }
  if ( ctx.ethernet_is_connected )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_PREPARE_SOCKET, NULL, 0 );
  }
  _change_state( IDLE );
}

static void _state_disabled_event_init_request( const app_event_t* event )
{
  bool result = true;
  app_event_t response = { 0 };
  AppEventPrepareWithData( &response, MSG_ID_INIT_RES, APP_EVENT_TCP_SERVER, APP_EVENT_NETWORK_MANAGER, &result, sizeof( result ) );
  NetworkManagerPostMsg( &response );
  _change_state( IDLE );
}

static void _state_idle_event_prepare_socket( const app_event_t* event )
{
  if ( ctx.server_socket != -1 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  ctx.server_socket = TCPTransport_CreateSocket();

  if ( ctx.server_socket < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  int rc = TCPTransport_Bind( ctx.server_socket );
  if ( rc < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  rc = TCPTransport_Listen( ctx.server_socket );
  if ( rc < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  _change_state( WAIT_CONNECTION );
  _send_internal_event( MSG_ID_TCP_SERVER_WAIT_CONNECTION, NULL, 0 );
}

static void _state_wait_connecting_event_wait_connection( const app_event_t* event )
{
  if ( ctx.ethernet_is_connected == false )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  int ret = TCPTransport_Select( ctx.server_socket, 1000 );

  if ( ret < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }
  else if ( ret == 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_WAIT_CONNECTION, NULL, 0 );
    return;
  }

  ret = TCPTransport_Accept( ctx.server_socket );
  if ( ret < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  ctx.client_socket = ret;
  //   keepAliveStart( &ctx.keepAlive );
  LOG( PRINT_INFO, "We have a new client connection! %d", ctx.client_socket );
  bool result = true;
  app_event_t response = { 0 };
  AppEventPrepareWithData( &response, MSG_ID_NETWORK_MANAGER_TCP_SERVER_CLIENT_STATUS, APP_EVENT_TCP_SERVER, APP_EVENT_NETWORK_MANAGER, &result, sizeof( result ) );
  NetworkManagerPostMsg( &response );
  _send_internal_event( MSG_ID_TCP_SERVER_WAIT_CLIENT_DATA, NULL, 0 );
  _change_state( WORKING );
}

static void _state_working_event_wait_client_data( const app_event_t* event )
{
  if ( ctx.ethernet_is_connected == false )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }

  int ret = TCPTransport_Select( ctx.client_socket, 100 );

  if ( ret < 0 )
  {
    _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    return;
  }
  else if ( ret > 0 )
  {
    ret = TCPTransport_Read( ctx.client_socket, (uint8_t*) ctx.payload, PAYLOAD_SIZE );
    if ( ret > 0 )
    {
      // const size_t payload_size = ret;
      /* ToDo parse response */
    }
    else
    {
      if ( ret == 0 )
      {
        LOG( PRINT_ERROR, "Client disconnected 0 %d", ctx.client_socket );
      }
      _send_internal_event( MSG_ID_TCP_SERVER_CLOSE_SOCKET, NULL, 0 );
    }
  }
  /* ToDo check send queue */
  _send_internal_event( MSG_ID_TCP_SERVER_WAIT_CLIENT_DATA, NULL, 0 );
}

//--------------------------------------------------------------------------------

int TCPServer_SendData( uint8_t* buff, size_t len )
{
  assert( ( buff != NULL ) && ( len != 0 ) );
  if ( ctx.state != WORKING )
  {
    LOG( PRINT_ERROR, "%s bad state", __func__ );
    return -1;
  }

  int ret = TCPTransport_Send( ctx.client_socket, buff, len );

  if ( ret < 0 )
  {
    LOG( PRINT_ERROR, "%s error send msg", __func__ );
  }

  return ret;
}

static void _task( void* pv )
{
  while ( 1 )
  {
    app_event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      AppEventSearchAndExecute( &event, module_state[ctx.state].event_handler_array, module_state[ctx.state].event_handler_array_size );
      AppEventDelete( &event );
    }
  }
}

/* Public functions -----------------------------------------------------------*/

void TCPServer_Init( void )
{
  ctx.client_socket = -1;
  ctx.server_socket = -1;
  ctx.queue = xQueueCreate( 8, sizeof( app_event_t ) );
  assert( ctx.queue );
  xTaskCreate( _task, "TCPServer", CONFIG_TCPIP_EVENT_THD_WA_SIZE, NULL, NORMALPRIOR, NULL );
}

void TCPServer_PostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}
