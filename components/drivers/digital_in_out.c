/**
 *******************************************************************************
 * @file    digital_in_out.c
 * @author  Dmytro Shevchenko
 * @brief   Digital in out source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "digital_in_out.h"

#include "app_config.h"
#include "driver/gpio.h"
#include "mqtt_json_parser.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[Dev] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_MQTT_APP
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_SIZE( _array ) sizeof( _array ) / sizeof( _array[0] )

/* Private functions declaration ---------------------------------------------*/

static void _set_output( void* user_data, bool value );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t output_tokens[] = {
  {.bool_cb = _set_output,
   .name = "value"},
};

/* Private functions ---------------------------------------------------------*/

static void IRAM_ATTR gpio_isr_handler( void* arg )
{
  digital_in_t* dev = (digital_in_t*) arg;
  dev->interrupt();
  // xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void _set_output( void* user_data, bool value )
{
  digital_out_t* dev = (digital_out_t*) user_data;
  LOG( PRINT_INFO, "%s %s %d", __func__, dev->name, value );
  DigitalOut_SetValue( dev, value );
}

static void _init_output( digital_out_t* dev )
{
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = ( 1ULL << dev->pin );
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 0;
  gpio_config( &io_conf );
}

static void _init_input( digital_in_t* dev )
{
  static bool is_interrupt_installed = false;
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_POSEDGE;
  io_conf.pin_bit_mask = ( 1ULL << dev->pin );
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en = 1;
  gpio_config( &io_conf );
  gpio_set_intr_type( dev->pin, GPIO_INTR_ANYEDGE );

  if ( is_interrupt_installed == false )
  {
    gpio_install_isr_service( 0 );
    is_interrupt_installed = true;
  }

  gpio_isr_handler_add( dev->pin, gpio_isr_handler, (void*) dev );
}

/* Public functions ---------------------------------------------------------*/

void DigitalIn_Init( digital_in_t* dev, const char* name, digital_in_interrupt_cb interrupt, uint8_t pin )
{
  dev->name = name;
  dev->interrupt = interrupt;
  dev->pin = pin;
  _init_input( dev );
}

error_code_t DigitalIn_ReadValue( digital_in_t* dev )
{
  dev->value = gpio_get_level( dev->pin );
  return ERROR_CODE_OK;
}

void DigitalOut_Init( digital_out_t* dev, const char* name, digital_set_value_cb set_value_callback, uint8_t pin )
{
  dev->name = name;
  dev->set_value = set_value_callback;
  dev->pin = pin;
  _init_output( dev );
  MQTTJsonParser_RegisterMethod( output_tokens, ARRAY_SIZE( output_tokens ), MQTT_TOPIC_TYPE_CONTROL,
                                 name, (void*) dev, NULL, NULL );
}

error_code_t DigitalOut_SetValue( digital_out_t* dev, bool value )
{
  assert( dev->set_value );
  error_code_t err = dev->set_value( value );
  if ( ERROR_CODE_OK == err )
  {
    dev->value = value;
    gpio_set_level( dev->pin, (uint32_t) dev->value );
  }
  return err;
}
