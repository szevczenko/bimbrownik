/**
 *******************************************************************************
 * @file    temperature.c
 * @author  Dmytro Shevchenko
 * @brief   Temperature driver
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "temperature.h"

#include "app_events.h"
#include "app_manager.h"
#include "app_timers.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ow/devices/ow_device_ds18x20.h"
#include "ow/ow.h"
#include "ow_esp32.h"
#include "pcf8574.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[Temp] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TEMPERATURE
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CONFIG_THD_SIZE 4096

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                        \
  STATE( DISABLED, _disabled_state_handler_array ) \
  STATE( INIT, _init_state_handler_array )         \
  STATE( IDLE, _idle_state_handler_array )         \
  STATE( SCANNING, _scanning_state_handler_array ) \
  STATE( WORKING, _working_state_handler_array )

/** @brief  Private types */
typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} drv_state_t;

typedef struct
{
  drv_state_t state;
  ow_t ow;
  ow_rom_t rom_ids[10];
  size_t rom_found;
  float avg_temp;
  size_t avg_temp_count;
  QueueHandle_t queue;
} drv_ctx_t;

typedef enum
{
  TIMER_ID_SCAN_TIMEOUT,
  TIMER_ID_MEASURE_REQ,
  TIMER_ID_LAST
} timer_id;

/* Private variables ---------------------------------------------------------*/

static drv_ctx_t ctx;

const ow_ll_drv_t ow_ll_drv_esp32 = {
  .init = OWUart_init,
  .deinit = OWUart_deinit,
  .set_baudrate = OWUart_setBaudrate,
  .tx_rx = OWUart_transmitReceive,
};

/* Private functions declaration ---------------------------------------------*/
static void _timeout_scan_cb( TimerHandle_t xTimer );
static void _measure_req_cb( TimerHandle_t xTimer );

static void _state_common_event_deinit_request( const app_event_t* event );

static void _state_disabled_event_init_request( const app_event_t* event );

static void _state_init_event_init_request( const app_event_t* event );
static void _state_init_event_init_response( const app_event_t* event );

static void _state_idle_event_scan_device_req( const app_event_t* event );
static void _state_idle_event_start_measure( const app_event_t* event );

// static void _state_scanning_event_start_measure( const app_event_t* event );
static void _state_scanning_event_scan_devices_req( const app_event_t* event );
static void _state_scanning_event_scan_devices_res( const app_event_t* event );

static void _state_working_event_scan_devices_req( const app_event_t* event );
static void _state_working_event_stop_measure( const app_event_t* event );
static void _state_working_event_measure_req( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
};

static const struct app_events_handler _init_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_init_event_init_request ),
    EVENT_ITEM( MSG_ID_INIT_RES, _state_init_event_init_response ),
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, _state_idle_event_scan_device_req ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_START_MEASURE, _state_idle_event_start_measure ),
};

static const struct app_events_handler _scanning_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
    // EVENT_ITEM( MSG_ID_TEMPERATURE_START_MEASURE, _state_scanning_event_start_measure ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, _state_scanning_event_scan_devices_req ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_SCAN_DEVICES_RES, _state_scanning_event_scan_devices_res ),
};

static const struct app_events_handler _working_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, _state_working_event_scan_devices_req ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_STOP_MEASURE, _state_working_event_stop_measure ),
    EVENT_ITEM( MSG_ID_TEMPERATURE_MEASURE_REQ, _state_working_event_measure_req ),
};

struct state_context
{
  const drv_state_t state;
  const char* name;
  const struct app_events_handler* event_handler_array;
  uint8_t event_handler_array_size;
};

static const struct state_context drv_state[STATE_TOP] =
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
    TIMER_ITEM( TIMER_ID_SCAN_TIMEOUT, _timeout_scan_cb, 1500, "ScanTimeout" ),
    TIMER_ITEM( TIMER_ID_MEASURE_REQ, _measure_req_cb, 1000, "MeasureReq" ) };

/* Private functions ---------------------------------------------------------*/

/** @brief  States functions. */
static const char* _get_state_name( const drv_state_t state )
{
  return drv_state[state].name;
}

static void _change_state( drv_state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_TEMP_DRV, APP_EVENT_TEMP_DRV );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_TEMP_DRV, APP_EVENT_TEMP_DRV, data, data_size );
  }

  TemperaturePostMsg( &event );
}

static void _timeout_scan_cb( TimerHandle_t xTimer )
{
  // _send_internal_event( MSG_ID_WIFI_TIMEOUT_CONNECT, NULL, 0 );
}

static void _measure_req_cb( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_TEMPERATURE_MEASURE_REQ, NULL, 0 );
}

/* Sate machine functions ---------------------------------------------------*/

static void _state_disabled_event_init_request( const app_event_t* event )
{
  _change_state( INIT );
  _send_internal_event( MSG_ID_INIT_REQ, NULL, 0 );
}

static void _state_common_event_deinit_request( const app_event_t* event )
{
}

static void _state_init_event_init_request( const app_event_t* event )
{
  temp_drv_err_t err = TEMP_DRV_ERR_OK;
  if ( owOK != ow_init( &ctx.ow, &ow_ll_drv_esp32, NULL ) )
  {
    err = TEMP_DRV_ERR_FAIL;
    LOG( PRINT_ERROR, "Init one wire driver failed" );
  }
  _send_internal_event( MSG_ID_INIT_RES, &err, sizeof( err ) );
}

static void _state_init_event_init_response( const app_event_t* event )
{
  temp_drv_err_t err = 0;
  AppEventGetData( event, &err, sizeof( temp_drv_err_t ) );
  app_event_t response = {};
  bool response_data = err == TEMP_DRV_ERR_OK ? true : false;

  AppEventPrepareWithData( &response, MSG_ID_INIT_RES, APP_EVENT_TEMP_DRV, APP_EVENT_APP_MANAGER, &response_data, sizeof( response_data ) );
  AppManagerPostMsg( &response );
  if ( TEMP_DRV_ERR_OK == err )
  {
    _change_state( IDLE );
  }
  else
  {
    _change_state( DISABLED );
  }
}

static void _state_idle_event_scan_device_req( const app_event_t* event )
{
  _change_state( SCANNING );
  _send_internal_event( MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, NULL, 0 );
}

static void _state_idle_event_start_measure( const app_event_t* event )
{
}

// static void _state_scanning_event_start_measure( const app_event_t* event )
// {
// }

static void _state_scanning_event_scan_devices_req( const app_event_t* event )
{
  temp_drv_err_t err = TEMP_DRV_ERR_OK;
  if ( owOK == ow_search_devices( &ctx.ow, ctx.rom_ids, OW_ARRAYSIZE( ctx.rom_ids ), &ctx.rom_found ) )
  {
    temp_drv_err_t err = TEMP_DRV_ERR_FAIL;
    LOG( PRINT_INFO, "Devices scanned, found %d devices!\r\n", (int) ctx.rom_found );
  }
  /* Sprawdzenie wszytkich czujników pokolei */
  _send_internal_event( MSG_ID_TEMPERATURE_SCAN_DEVICES_RES, &err, sizeof( err ) );
}

static void _state_scanning_event_scan_devices_res( const app_event_t* event )
{
  temp_drv_err_t err = TEMP_DRV_ERR_OK;
  AppEventGetData( event, &err, sizeof( temp_drv_err_t ) );
  app_event_t response = {};

  AppEventPrepareWithData( &response, MSG_ID_APP_MANAGER_TEMP_SENSORS_SCAN_RES, APP_EVENT_TEMP_DRV, APP_EVENT_APP_MANAGER, &err, sizeof( err ) );
  AppManagerPostMsg( &response );
  if ( err == TEMP_DRV_ERR_OK )
  {
    // _send_internal_event( MSG_ID_TEMPERATURE_MEASURE_REQ, NULL, 0 );
    _change_state( WORKING );
  }
  else
  {
    _change_state( IDLE );
  }
}

static void _state_working_event_scan_devices_req( const app_event_t* event )
{
  _change_state( SCANNING );
  _send_internal_event( MSG_ID_TEMPERATURE_SCAN_DEVICES_REQ, NULL, 0 );
}

static void _state_working_event_stop_measure( const app_event_t* event )
{
  _change_state( IDLE );
  AppTimerStop( timers, TIMER_ID_MEASURE_REQ );
}

static void _state_working_event_measure_req( const app_event_t* event )
{
  /* Start measure */
  temp_drv_err_t err = TEMP_DRV_ERR_OK;
  app_event_t response = {};

  AppEventPrepareWithData( &response, MSG_ID_APP_MANAGER_TEMP_SENSORS_SCAN_RES, APP_EVENT_TEMP_DRV, APP_EVENT_APP_MANAGER, &err, sizeof( err ) );
  AppManagerPostMsg( &response );

  if ( err == TEMP_DRV_ERR_OK )
  {
    AppTimerStart( timers, TIMER_ID_MEASURE_REQ );
  }
  else
  {
    _change_state( IDLE );
  }
}

static void _event_task( void* pv )
{
  while ( 1 )
  {
    app_event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      AppEventSearchAndExecute( &event, drv_state[ctx.state].event_handler_array, drv_state[ctx.state].event_handler_array_size );
      AppEventDelete( &event );
    }
  }
}

/* Public functions -----------------------------------------------------------*/

void TemperaturePostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void TemperatureInit( void )
{
  ctx.queue = xQueueCreate( 8, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _event_task, "temperature", CONFIG_THD_SIZE, NULL, NORMALPRIOR, NULL );
}
