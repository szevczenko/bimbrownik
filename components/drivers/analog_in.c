/**
 *******************************************************************************
 * @file    error_code.c
 * @author  Dmytro Shevchenko
 * @brief   Error code
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "analog_in.h"

#include <string.h>

#include "app_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[Dev] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_MQTT_APP
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private variables ---------------------------------------------------------*/

/* Public functions ---------------------------------------------------------*/

void AnalogIn_Init( analog_in_t* dev, const char* name, const char* unit, analog_init_cb init,
                    analog_read_value_cb read_value, analog_deinit_cb deinit )
{
  dev->name = name;
  dev->unit = unit;
  dev->init = init;
  dev->read_value = read_value;
  dev->deinit = deinit;
}

error_code_t AnalogIn_ReadValue( analog_in_t* dev )
{
  assert( dev->read_value );
  error_code_t err = dev->read_value( &dev->value );
  return err;
}
