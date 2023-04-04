/**
 *******************************************************************************
 * @file    network_manager.c
 * @author  Dmytro Shevchenko
 * @brief   Network manager
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "network_manager.h"

#include "app_events.h"
#include "app_manager.h"
#include "app_timers.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
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
  STATE( IDLE, _idle_state_handler_array )

/** @brief  Private types */
typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} module_state_t;

typedef struct
{
  module_state_t state;
  uint32_t modules_init;

  bool wifi_init_status;
  bool wifi_is_read_connecting_data;
  bool wifi_is_connected;
  QueueHandle_t queue;
} module_ctx_t;

typedef enum
{
  TIMER_ID_TIMEOUT_INIT,
  TIMER_ID_LAST
} timer_id;

/* Private variables ---------------------------------------------------------*/

static module_ctx_t ctx;

/* Private functions declaration ---------------------------------------------*/
static void _timeout_init_cb( TimerHandle_t xTimer );

static void _state_disabled_event_init_request( const app_event_t* event );

static void _state_init_event_init_request( const app_event_t* event );
static void _state_init_event_init_response( const app_event_t* event );
static void _state_init_event_init_module_response( const app_event_t* event );
static void _state_init_event_timeout_init( const app_event_t* event );
static void _state_init_event_wifi_connect_req( const app_event_t* event );
static void _state_init_event_wifi_connect_res( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
};

static const struct app_events_handler _init_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_init_event_init_request ),
    EVENT_ITEM( MSG_ID_NETWORK_MANAGER_INIT_RES, _state_init_event_init_response ),
    EVENT_ITEM( MSG_ID_NETWORK_MANAGER_TIMEOUT_INIT, _state_init_event_timeout_init ),
    EVENT_ITEM( MSG_ID_NETWORK_MANAGER_WIFI_CONNECT_REQ, _state_init_event_timeout_init ),
    EVENT_ITEM( MSG_ID_NETWORK_MANAGER_WIFI_CONNECT_RES, _state_init_event_timeout_init ),
    EVENT_ITEM( MSG_ID_INIT_RES, _state_init_event_init_module_response ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
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
    TIMER_ITEM( TIMER_ID_TIMEOUT_INIT, _timeout_init_cb, 1000, "NetworkTimeoutInit" ) };

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

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
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

static void _timeout_init_cb( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_NETWORK_MANAGER_TIMEOUT_INIT, NULL, 0 );
}

/* Sate machine functions ---------------------------------------------------*/

static void _state_disabled_event_init_request( const app_event_t* event )
{
  _change_state( INIT );
  _send_internal_event( MSG_ID_INIT_REQ, NULL, 0 );
}

static void _state_init_event_init_request( const app_event_t* event )
{
  ctx.modules_init = 0;
  app_event_t event_send = { 0 };

  /* WiFi*/
  AppEventPrepareNoData( &event_send, MSG_ID_INIT_REQ, APP_EVENT_NETWORK_MANAGER, APP_EVENT_WIFI_DRV );
  WifiDrvPostMsg( &event_send );

  AppTimerStart( timers, TIMER_ID_TIMEOUT_INIT );
}

static void _state_init_event_init_response( const app_event_t* event )
{
  bool result = false;
  app_event_t response = { 0 };
  if ( ctx.wifi_init_status )
  {
    _change_state( IDLE );
    if ( ctx.wifi_is_read_connecting_data )
    {
      app_event_t event_send = { 0 };
      AppEventPrepareNoData( &event_send, MSG_ID_WIFI_CONNECT_REQ, APP_EVENT_NETWORK_MANAGER, APP_EVENT_WIFI_DRV );
      WifiDrvPostMsg( &event_send );
    }
    result = true;
  }
  else
  {
    _change_state( DISABLED );
    AppEventPrepareNoData( &response, MSG_ID_DEINIT_REQ, APP_EVENT_NETWORK_MANAGER, APP_EVENT_WIFI_DRV );
    WifiDrvPostMsg( &response );
  }
  AppEventPrepareWithData( &response, MSG_ID_INIT_RES, APP_EVENT_NETWORK_MANAGER, APP_EVENT_APP_MANAGER, &result, sizeof( result ) );
  AppManagerPostMsg( &response );

  AppTimerStop( timers, TIMER_ID_TIMEOUT_INIT );
}

static void _state_init_event_init_module_response( const app_event_t* event )
{
  if ( event->src == APP_EVENT_WIFI_DRV )
  {
    ctx.modules_init++;
    wifi_drv_err_t err = 0;
    if ( AppEventGetData( event, &err, sizeof( err ) ) == false )
    {
      LOG( PRINT_ERROR, "%s Cannot get data from event", __func__ );
      _change_state( DISABLED );
      return;
    }

    if ( err == WIFI_DRV_ERR_OK )
    {
      ctx.wifi_init_status = true;
      ctx.wifi_is_read_connecting_data = true;
    }
    else if ( err == WIFI_DRV_ERR_WIFI_MEMORY_EMPTY )
    {
      ctx.wifi_init_status = true;
      ctx.wifi_is_read_connecting_data = false;
    }
    else
    {
      ctx.wifi_init_status = false;
      ctx.wifi_is_read_connecting_data = false;
    }
  }

  if ( ctx.modules_init == 1 )
  {
    _send_internal_event( MSG_ID_NETWORK_MANAGER_INIT_RES, NULL, 0 );
  }
}

static void _state_init_event_timeout_init( const app_event_t* event )
{
  _send_internal_event( MSG_ID_NETWORK_MANAGER_INIT_RES, NULL, 0 );
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

void NetworkManagerPostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void NetworkManagerInit( void )
{
  ctx.queue = xQueueCreate( 16, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _task, "network_manager_task", 4096, NULL, NORMALPRIOR, NULL );
}
