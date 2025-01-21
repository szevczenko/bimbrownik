/**
 *******************************************************************************
 * @file    network_manager.c
 * @author  Dmytro Shevchenko
 * @brief   Network manager
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "network_manager.h"

#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hawkbit_process.h"
#include "http_server.h"
#include "mdns_service.h"
#include "mqtt_app.h"
#include "wifi_http_app.h"
#include "wifidrv.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[NetworkManager] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_NETWORK_MANAGER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                        \
  STATE( DISABLED, _disabled_state_handler_array ) \
  STATE( INIT, _init_state_handler_array )         \
  STATE( SERVER, _server_state_handler_array )     \
  STATE( CLIENT, _client_state_handler_array )

/** @brief  Private types */
typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} module_state_t;

typedef enum
{
  WIFI_STOP,
  WIFI_CLIENT,
  WIFI_SERVER,
} wifi_state_t;

typedef struct
{
  module_state_t state;
  uint32_t modules_init;

  bool is_connected;
  wifi_state_t wifi_state;
  /* This flag for do not send REQUEST_ERROR_CONNECT during recconect */
  bool is_disconnect_req;

  QueueHandle_t queue;
} module_ctx_t;

typedef enum
{
  TIMER_ID_DISABLE_AP,
  TIMER_ID_LAST
} timer_id;

typedef enum
{
  WIFI_DRV_ERR_OK,
  WIFI_DRV_ERR_CONNECTED,
  WIFI_DRV_ERR_DISCONNECTED,
  WIFI_DRV_ERR_FAIL,
  WIFI_DRV_ERR_LAST
} wifi_drv_err_t;

typedef enum
{
  REQUEST_INIT,
  REQUEST_START_CLIENT,
  REQUEST_CONNECT,
  REQUEST_CONNECTED,
  REQUEST_START_SERVER,
  REQUEST_ERROR_CONNECT,
} request_t;

/* Private variables ---------------------------------------------------------*/

static module_ctx_t ctx;

extern void API_Init( void );

/* Private functions declaration ---------------------------------------------*/
static void _disable_ap_cb( TimerHandle_t xTimer );

static void _state_disabled_request_init( const app_event_t* event );

static void _state_init_request_init( const app_event_t* event );

static void _state_common_request_connect( const app_event_t* event );

static void _state_client_request_start_client( const app_event_t* event );
static void _state_client_request_connected( const app_event_t* event );
static void _state_client_request_start_server( const app_event_t* event );
static void _state_client_request_error_connect( const app_event_t* event );

static void _state_server_request_connected( const app_event_t* event );
static void _state_server_request_start_client( const app_event_t* event );
static void _state_server_request_start_server( const app_event_t* event );
static void _state_server_request_error_connect( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( REQUEST_INIT, _state_disabled_request_init ),
};

static const struct app_events_handler _init_state_handler_array[] =
  {
    EVENT_ITEM( REQUEST_INIT, _state_init_request_init ),
};

static const struct app_events_handler _client_state_handler_array[] =
  {
    EVENT_ITEM( REQUEST_START_CLIENT, _state_client_request_start_client ),
    EVENT_ITEM( REQUEST_CONNECT, _state_common_request_connect ),
    EVENT_ITEM( REQUEST_CONNECTED, _state_client_request_connected ),
    EVENT_ITEM( REQUEST_START_SERVER, _state_client_request_start_server ),
    EVENT_ITEM( REQUEST_ERROR_CONNECT, _state_client_request_error_connect ),
};

static const struct app_events_handler _server_state_handler_array[] =
  {
    EVENT_ITEM( REQUEST_CONNECT, _state_common_request_connect ),
    EVENT_ITEM( REQUEST_CONNECTED, _state_server_request_connected ),
    EVENT_ITEM( REQUEST_START_SERVER, _state_server_request_start_server ),
    EVENT_ITEM( REQUEST_ERROR_CONNECT, _state_server_request_error_connect ),
    EVENT_ITEM( REQUEST_START_CLIENT, _state_server_request_start_client ),
};

struct state_context
{
  const module_state_t state;
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

static app_timer_t timers[] =
  {
    TIMER_ITEM( TIMER_ID_DISABLE_AP, _disable_ap_cb, 5000, "NetworkTimeoutInit" ) };

/* Private functions ---------------------------------------------------------*/

/** @brief  States functions. */
static const char* _get_state_name( const module_state_t state )
{
  return module_state[state].name;
}

static void _change_state( module_state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( request_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_NETWORK_MANAGER, APP_EVENT_NETWORK_MANAGER );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_NETWORK_MANAGER, APP_EVENT_NETWORK_MANAGER, data, data_size );
  }

  NetworkManagerPostMsg( &event );
}

static void _wifi_get_ip_address_cb( void )
{
  _send_internal_event( REQUEST_CONNECTED, NULL, 0 );
}

static void _wifi_get_disconnected_cb( void )
{
  if ( ctx.is_disconnect_req == false )
  {
    _send_internal_event( REQUEST_ERROR_CONNECT, NULL, 0 );
  }
}

static void _disable_ap_cb( TimerHandle_t xTimer )
{
  _send_internal_event( REQUEST_START_CLIENT, NULL, 0 );
}

static void _start_client_services( void )
{
  // app_event_t tcp_event = { 0 };
  // app_event_t hawkbit_event = { 0 };
  // app_event_t mqtt_event = { 0 };

  // AppEventPrepareNoData( &tcp_event, MSG_ID_TCP_SERVER_ETHERNET_CONNECTED, APP_EVENT_NETWORK_MANAGER, APP_EVENT_TCP_SERVER );
  // AppEventPrepareNoData( &hawkbit_event, MSG_ID_HAWKBIT_POLL_SERVER, APP_EVENT_NETWORK_MANAGER, APP_EVENT_HAWKBIT );
  // AppEventPrepareNoData( &mqtt_event, MSG_ID_MQTT_ETH_CONNECTED, APP_EVENT_NETWORK_MANAGER, APP_EVENT_MQTT_APP );

  // TCPServer_PostMsg( &tcp_event );
  // HawkbitProcess_PostMsg( &hawkbit_event );
  MqttApp_Init();
  HTTPServer_Init();
  mDNS_Start();
  HawkbitProcess_Init();
}

static void _stop_client_services( void )
{
  // app_event_t tcp_event = { 0 };
  // app_event_t hawkbit_event = { 0 };
  // app_event_t mqtt_event = { 0 };

  // AppEventPrepareNoData( &tcp_event, MSG_ID_TCP_SERVER_ETHERNET_DISCONNECTED, APP_EVENT_NETWORK_MANAGER, APP_EVENT_TCP_SERVER );
  // AppEventPrepareNoData( &hawkbit_event, MSG_ID_HAWKBIT_STOP_POLL_SERVER, APP_EVENT_NETWORK_MANAGER, APP_EVENT_HAWKBIT );
  // AppEventPrepareNoData( &mqtt_event, MSG_ID_MQTT_ETH_DISCONNECTED, APP_EVENT_NETWORK_MANAGER, APP_EVENT_MQTT_APP );

  // TCPServer_PostMsg( &tcp_event );
  // HawkbitProcess_PostMsg( &hawkbit_event );
  MqttApp_Deinit();
  HTTPServer_Deinit();
  mDNS_Stop();
  HawkbitProcess_Deinit();
}

static void _start_server_services( void )
{
  WiFiHTTPApp_Start();
}

static void _stop_server_services( void )
{
  WiFiHTTPApp_Stop();
}

/* Sate machine functions ---------------------------------------------------*/

static void _state_disabled_request_init( const app_event_t* event )
{
  _send_internal_event( REQUEST_INIT, NULL, 0 );
  _change_state( INIT );
}

static void _state_init_request_init( const app_event_t* event )
{
  if ( wifiDrvIsReadData() == true )
  {
    _change_state( CLIENT );
    _send_internal_event( REQUEST_START_CLIENT, NULL, 0 );
  }
  else
  {
    _change_state( SERVER );
    _send_internal_event( REQUEST_START_SERVER, NULL, 0 );
  }
  ctx.is_connected = false;
}

static void _state_common_request_connect( const app_event_t* event )
{
  wifiConData_t data = { 0 };
  bool is_get_data = AppEventGetData( event, &data, sizeof( data ) );
  if ( is_get_data )
  {
    wifiDrvSetAPName( data.ssid, strlen( data.ssid ) );
    wifiDrvSetPassword( data.password, strlen( data.password ) );
  }
  if ( ctx.is_connected == true )
  {
    _stop_client_services();
    ctx.is_disconnect_req = true;
    wifiDrvDisconnect();
    /* Wait to disconnect */
    int cnt = 0;
    while ( wifiDrvReadyToConnect() == false && cnt < 30 )
    {
      vTaskDelay( MS2ST( 100 ) );
    }
    assert( wifiDrvReadyToConnect() );
    ctx.is_disconnect_req = false;
  }
  wifiDrvConnect();
}

static void _state_client_request_start_client( const app_event_t* event )
{
  wifiConData_t data = { 0 };
  bool is_get_data = AppEventGetData( event, &data, sizeof( data ) );

  if ( ctx.wifi_state != WIFI_CLIENT )
  {
    wifiDrvSetWifiType( T_WIFI_TYPE_CLIENT );
    wifiDrvStart();
    ctx.wifi_state = WIFI_CLIENT;
  }

  if ( is_get_data )
  {
    wifiDrvSetAPName( data.ssid, strlen( data.ssid ) );
    wifiDrvSetPassword( data.password, strlen( data.password ) );
  }
  ctx.is_connected = false;
  _send_internal_event( REQUEST_CONNECT, NULL, 0 );
}

static void _state_client_request_connected( const app_event_t* event )
{
  ctx.is_connected = true;
  _start_client_services();
}

static void _state_client_request_start_server( const app_event_t* event )
{
  ctx.wifi_state = WIFI_STOP;
  wifiDrvStop();
  _stop_client_services();
  _change_state( SERVER );
  _send_internal_event( REQUEST_START_SERVER, NULL, 0 );
  ctx.is_connected = false;
}

static void _state_client_request_error_connect( const app_event_t* event )
{
  _stop_client_services();
  if ( ctx.is_connected == false )
  {
    _send_internal_event( REQUEST_START_SERVER, NULL, 0 );
  }
  else
  {
    _send_internal_event( REQUEST_CONNECT, NULL, 0 );
    ctx.is_connected = false;
  }
}

static void _state_server_request_connected( const app_event_t* event )
{
  ctx.is_connected = true;
  AppTimerStart( timers, TIMER_ID_DISABLE_AP );
}

static void _state_server_request_start_client( const app_event_t* event )
{
  ctx.wifi_state = WIFI_STOP;
  ctx.is_connected = false;
  _stop_server_services();
  wifiDrvStop();
  _change_state( CLIENT );
  _send_internal_event( REQUEST_START_CLIENT, NULL, 0 );
}

static void _state_server_request_start_server( const app_event_t* event )
{
  if ( ctx.wifi_state != WIFI_SERVER )
  {
    wifiDrvSetWifiType( T_WIFI_TYPE_CLI_SER );
    wifiDrvStart();
    ctx.wifi_state = WIFI_SERVER;
  }

  _start_server_services();
}

static void _state_server_request_error_connect( const app_event_t* event )
{
  ctx.is_connected = false;
  AppTimerStop( timers, TIMER_ID_DISABLE_AP );
}

static void _task( void* pv )
{
  wifiDrvRegisterConnectCb( _wifi_get_ip_address_cb );
  wifiDrvRegisterDisconnectCb( _wifi_get_disconnected_cb );
  _send_internal_event( REQUEST_INIT, NULL, 0 );
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

void NetworkManagerPostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void NetworkManagerInit( void )
{
  API_Init();
  ctx.queue = xQueueCreate( 16, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _task, "network_manager_task", 4096, NULL, NORMALPRIOR, NULL );
}
