/**
 *******************************************************************************
 * @file    water_flow_sensor.c
 * @author  Dmytro Shevchenko
 * @brief   Water flow sensor source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "water_flow_sensor.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "mqtt_json_parser.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[Dev] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_DEVICE_MANAGER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_SIZE( _array ) sizeof( _array ) / sizeof( _array[0] )

/* Private functions declaration ---------------------------------------------*/

static void _set_alert( void* user_data, bool value );
static void _set_reset( void* user_data, bool value );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t output_tokens[] = {
  {.bool_cb = _set_alert,
   .name = "alert"},
  { .bool_cb = _set_reset,
   .name = "reset"},
};

/* Private functions ---------------------------------------------------------*/

static void IRAM_ATTR gpio_isr_handler( void* arg )
{
  water_flow_sensor_t* dev = (water_flow_sensor_t*) arg;
  dev->counter++;
  dev->value = 0.75 * dev->counter;
  if ( dev->value >= dev->alert_value )
  {
    dev->alert_cb( dev->value );
  }
}

static void _set_alert( void* user_data, bool value )
{
  water_flow_sensor_t* dev = (water_flow_sensor_t*) user_data;
  LOG( PRINT_INFO, "%s %s %d", __func__, dev->name, value );
  WaterFlowSensor_SetAlertValue( dev, value );
}

static void _set_reset( void* user_data, bool value )
{
  water_flow_sensor_t* dev = (water_flow_sensor_t*) user_data;
  LOG( PRINT_INFO, "%s %s %d", __func__, dev->name, value );
  WaterFlowSensor_ResetValue( dev );
}

static void _init_sensor( water_flow_sensor_t* dev )
{
  static bool is_interrupt_installed = false;
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  io_conf.pin_bit_mask = ( 1ULL << dev->pin );
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  gpio_config( &io_conf );
  gpio_set_intr_type( dev->pin, GPIO_INTR_POSEDGE );

  if ( is_interrupt_installed == false )
  {
    gpio_install_isr_service( 0 );
    is_interrupt_installed = true;
  }

  gpio_isr_handler_add( dev->pin, gpio_isr_handler, (void*) dev );
}

/* Public functions ---------------------------------------------------------*/

void WaterFlowSensor_Init( water_flow_sensor_t* dev, const char* name, const char* unit, water_flow_sensor_alert_cb alert_cb, uint8_t pin )
{
  dev->name = name;
  dev->alert_cb = alert_cb;
  dev->pin = pin;
  dev->unit = unit;
  dev->alert_value = UINT32_MAX;
  _init_sensor( dev );
  MQTTJsonParser_RegisterMethod( output_tokens, ARRAY_SIZE( output_tokens ), MQTT_TOPIC_TYPE_CONTROL,
                                 name, (void*) dev, NULL, NULL );
}

error_code_t WaterFlowSensor_SetAlertValue( water_flow_sensor_t* dev, uint32_t alert_value )
{
  dev->alert_value = alert_value;
  return ERROR_CODE_OK;
}

error_code_t WaterFlowSensor_ResetValue( water_flow_sensor_t* dev )
{
  dev->value = 0;
  return ERROR_CODE_OK;
}

size_t WaterFlowSensor_GetStr( water_flow_sensor_t* dev, char* buffer, size_t buffer_size, bool is_add_comma )
{
  if ( is_add_comma )
  {
    return snprintf( buffer, buffer_size, ",\"%s\":%ld", dev->name, dev->value );
  }
  else
  {
    return snprintf( buffer, buffer_size, "\"%s\":%ld", dev->name, dev->value );
  }
}
