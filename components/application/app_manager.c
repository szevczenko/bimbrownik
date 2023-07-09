/**
 *******************************************************************************
 * @file    app_manager.c
 * @author  Dmytro Shevchenko
 * @brief   Application manager
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "app_manager.h"

#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "modules.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[AppManager] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_APP_MANAGER
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

static void _state_common_temp_sensor_scan_res( const app_event_t* event );

static void _state_disabled_event_init_request( const app_event_t* event );

static void _state_init_event_init_request( const app_event_t* event );
static void _state_init_event_init_response( const app_event_t* event );
static void _state_init_event_init_module_response( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_APP_MANAGER_INIT_REQ, _state_disabled_event_init_request ),
};

static const struct app_events_handler _init_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_APP_MANAGER_INIT_REQ, _state_init_event_init_request ),
    EVENT_ITEM( MSG_ID_APP_MANAGER_INIT_RES, _state_init_event_init_response ),
    EVENT_ITEM( MSG_ID_INIT_RES, _state_init_event_init_module_response ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
    EVENT_ITEM( MSG_ID_APP_MANAGER_TEMP_SENSORS_SCAN_RES, _state_common_temp_sensor_scan_res ),
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
    TIMER_ITEM( TIMER_ID_TIMEOUT_INIT, _timeout_init_cb, 1500, "AppTimeoutInit" ),
};

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
    AppEventPrepareNoData( &event, id, APP_EVENT_APP_MANAGER, APP_EVENT_APP_MANAGER );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_APP_MANAGER, APP_EVENT_APP_MANAGER, data, data_size );
  }

  AppManagerPostMsg( &event );
}

static void _timeout_init_cb( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_APP_MANAGER_TIMEOUT_INIT, NULL, 0 );
}

/* State machine functions ---------------------------------------------------*/

static void _state_disabled_event_init_request( const app_event_t* event )
{
  _change_state( INIT );
  _send_internal_event( MSG_ID_APP_MANAGER_INIT_REQ, NULL, 0 );
}

static void _state_init_event_init_request( const app_event_t* event )
{
  ctx.modules_init = 0;
  app_event_t event_send = { 0 };
  for ( int i = 0; i < ARRAY_SIZE( modules ); i++ )
  {
    AppEventPrepareNoData( &event_send, modules[i].init_msg_id, APP_EVENT_APP_MANAGER, modules[i].task_id );
    modules[i].post_msg( &event_send );
  }
  AppTimerStart( timers, TIMER_ID_TIMEOUT_INIT );
}

static void _state_init_event_init_response( const app_event_t* event )
{
  app_manager_err_t err_code = 0;
  if ( AppEventGetData( event, &err_code, sizeof( err_code ) ) == false )
  {
    assert( 0 );
    return;
  }

  AppTimerStop( timers, TIMER_ID_TIMEOUT_INIT );

  if ( err_code == APP_MANAGER_ERR_OK )
  {
    /* For tests */
    app_event_t event_send = { 0 };
    AppEventPrepareNoData( &event_send, MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, APP_EVENT_APP_MANAGER, APP_EVENT_TEMP_DRV );
    TemperaturePostMsg( &event_send );
    // _send_internal_event( MSG_ID_APP_MANAGER_TEMP_WPS_TEST, NULL, 0 );
    /* --------- */
    _change_state( IDLE );
    return;
  }
  assert( 0 );
}

static void _state_init_event_init_module_response( const app_event_t* event )
{
  bool result = false;
  if ( AppEventGetData( event, &result, sizeof( result ) ) == false )
  {
    assert( 0 );
    return;
  }

  for ( int i = 0; i < ARRAY_SIZE( modules ); i++ )
  {
    if ( modules[i].task_id == event->src )
    {
      modules[i].init_result = result;
      ctx.modules_init++;
      break;
    }
  }

  if ( ctx.modules_init >= ARRAY_SIZE( modules ) )
  {
    result = true;
    for ( int i = 0; i < ARRAY_SIZE( modules ); i++ )
    {
      result &= modules[i].init_result;
    }
    app_manager_err_t err_code = result ? APP_MANAGER_ERR_OK : APP_MANAGER_FAIL_OPERATION;
    _send_internal_event( MSG_ID_APP_MANAGER_INIT_RES, &err_code, sizeof( err_code ) );
  }
}

static void _state_common_temp_sensor_scan_res( const app_event_t* event )
{
  /* ToDo: process event data */
}

static void _task( void* pv )
{
  _send_internal_event( MSG_ID_APP_MANAGER_INIT_REQ, NULL, 0 );
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

void AppManagerPostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void AppManagerInit( void )
{
  ctx.queue = xQueueCreate( 16, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _task, "app_manager_task", 4096, NULL, NORMALPRIOR, NULL );
}
