/**
 *******************************************************************************
 * @file    device_manager.c
 * @author  Dmytro Shevchenko
 * @brief   Device manager
 *******************************************************************************
 */
#include "device_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "analog_in.h"
#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "digital_in_out.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_app.h"
#include "water_flow_sensor.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[DM] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_DEVICE_MANAGER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                        \
  STATE( DISABLED, _disabled_state_handler_array ) \
  STATE( IDLE, _idle_state_handler_array )

typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} module_state_t;

typedef struct
{
  digital_in_t digital_inputs[2];
  digital_out_t digital_outs[2];
  analog_in_t analog_inputs[2];
  water_flow_sensor_t water_flow[1];
} devices_t;

typedef struct
{
  module_state_t state;
  QueueHandle_t queue;
  uint32_t measure_interval_ms;
  uint32_t post_data_interval_ms;
  devices_t devices;
  error_code_t measure_result;
  uint32_t iterator;
  char buffer[512];
} module_ctx_t;

typedef enum
{
  TIMER_ID_MEASURE,
  TIMER_ID_POST,
  TIMER_ID_LAST
} timer_id;

/* Private functions declaration ---------------------------------------------*/
static void _timer_measure( TimerHandle_t xTimer );
static void _timer_post( TimerHandle_t xTimer );

static void _state_disabled_init( const app_event_t* event );

static void _state_idle_event_measure( const app_event_t* event );
static void _state_idle_event_post( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_init ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_DEV_MANAGER_MEASURE, _state_idle_event_measure ),
    EVENT_ITEM( MSG_ID_DEV_MANAGER_POST, _state_idle_event_post ),
};

/* Private variables ---------------------------------------------------------*/

static module_ctx_t ctx;

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
    TIMER_ITEM( TIMER_ID_MEASURE, _timer_measure, 1000, "dm_meas" ),
    TIMER_ITEM( TIMER_ID_POST, _timer_post, 10000, "dm_post" ),
};

/* Private functions ---------------------------------------------------------*/

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
    AppEventPrepareNoData( &event, id, APP_EVENT_DEV_MANAGER, APP_EVENT_DEV_MANAGER );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_DEV_MANAGER, APP_EVENT_DEV_MANAGER, data, data_size );
  }

  DeviceManager_PostMsg( &event );
}

static void _timer_measure( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_DEV_MANAGER_MEASURE, NULL, 0 );
}

static void _timer_post( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_DEV_MANAGER_POST, NULL, 0 );
}

static error_code_t valve1_set( bool value )
{
  return ERROR_CODE_OK;
}

static error_code_t valve2_set( bool value )
{
  return ERROR_CODE_OK;
}

static void input1_read( void )
{
}

static void input2_read( void )
{
}

static error_code_t analog1_init( void )
{
  return ERROR_CODE_OK;
}

static error_code_t analog1_read( uint32_t* value )
{
  *value = 123;
  return ERROR_CODE_OK;
}

static error_code_t analog2_read( uint32_t* value )
{
  *value = 321;
  return ERROR_CODE_OK;
}

static error_code_t analog1_deinit( void )
{
  return ERROR_CODE_OK;
}

static void _alert_water_flow( uint32_t value )
{
}

static void _init_devices( void )
{
  DigitalOut_Init( &ctx.devices.digital_outs[0], "valve1", valve1_set, 2 );
  DigitalOut_Init( &ctx.devices.digital_outs[1], "valve2", valve2_set, 4 );
  DigitalIn_Init( &ctx.devices.digital_inputs[0], "input1", input1_read, 18 );
  DigitalIn_Init( &ctx.devices.digital_inputs[1], "input2", input2_read, 19 );
  AnalogIn_Init( &ctx.devices.analog_inputs[0], "t1", "'C", analog1_init, analog1_read, analog1_deinit );
  AnalogIn_Init( &ctx.devices.analog_inputs[1], "t2", "m", analog1_init, analog2_read, analog1_deinit );
  WaterFlowSensor_Init( &ctx.devices.water_flow[0], "v1_flow", "l", _alert_water_flow, 18 );
}

static void _state_disabled_init( const app_event_t* event )
{
  _change_state( IDLE );
  _init_devices();
  _send_internal_event( MSG_ID_DEV_MANAGER_MEASURE, NULL, 0 );
  _send_internal_event( MSG_ID_DEV_MANAGER_POST, NULL, 0 );
}

static void _state_idle_event_measure( const app_event_t* event )
{
  error_code_t error = ERROR_CODE_OK;
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.digital_inputs ); i++ )
  {
    error = DigitalIn_ReadValue( &ctx.devices.digital_inputs[i] );
    if ( error != ERROR_CODE_OK )
    {
      ctx.measure_result = error;
      break;
    }
  }
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.analog_inputs ); i++ )
  {
    error = AnalogIn_ReadValue( &ctx.devices.analog_inputs[i] );
    if ( error != ERROR_CODE_OK )
    {
      ctx.measure_result = error;
      break;
    }
  }
  AppTimerStart( timers, TIMER_ID_MEASURE );
}

static void _state_idle_event_post( const app_event_t* event )
{
  uint64_t timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
  memset( ctx.buffer, 0, sizeof( ctx.buffer ) );
  int offset = snprintf( ctx.buffer, sizeof( ctx.buffer ), "{\"s\":%ld,\"t\":%lld,\"e\":%d", ctx.iterator++, timestamp, ctx.measure_result );
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.digital_inputs ); i++ )
  {
    offset += snprintf( &ctx.buffer[offset], sizeof( ctx.buffer ) - offset, ",\"%s\":%s",
                        ctx.devices.digital_inputs[i].name, ctx.devices.digital_inputs[i].value ? "true" : "false" );
  }
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.analog_inputs ); i++ )
  {
    offset += snprintf( &ctx.buffer[offset], sizeof( ctx.buffer ) - offset, ",\"%s\":%ld",
                        ctx.devices.analog_inputs[i].name, ctx.devices.analog_inputs[i].value );
  }
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.digital_outs ); i++ )
  {
    offset += snprintf( &ctx.buffer[offset], sizeof( ctx.buffer ) - offset, ",\"%s\":%s",
                        ctx.devices.digital_outs[i].name, ctx.devices.digital_outs[i].value ? "true" : "false" );
  }
  for ( int i = 0; i < ARRAY_SIZE( ctx.devices.water_flow ); i++ )
  {
    offset += WaterFlowSensor_GetStr( &ctx.devices.water_flow[i], &ctx.buffer[offset], sizeof( ctx.buffer ) - offset, true );
  }
  offset += snprintf( &ctx.buffer[offset], sizeof( ctx.buffer ) - offset, "}" );
  MqttApp_PostData( "test", ctx.buffer );
  AppTimerStart( timers, TIMER_ID_POST );
}

static void _task( void* pv )
{
  _send_internal_event( MSG_ID_INIT_REQ, NULL, 0 );
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

void DeviceManager_PostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void DeviceManager_Init( void )
{
  ctx.queue = xQueueCreate( 8, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _task, "device_manager", 3072, NULL, NORMALPRIOR, NULL );
}