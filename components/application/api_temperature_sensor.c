/**
 *******************************************************************************
 * @file    api_temperature_sensor.c
 * @author  Dmytro Shevchenko
 * @brief   Communication API temeperature sensor
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "app_config.h"
#include "json_parser.h"
#include "temperature.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[API Temp] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_LEN( _array ) sizeof( _array ) / sizeof( _array[0] )

/* Private types -------------------------------------------------------------*/

/* Private functions declaration ---------------------------------------------*/

static void _set_temperature_sensor( int sensor_number, uint32_t iterator );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t temperature_sensor_tokens[] = {
  {.int_cb = _set_temperature_sensor,
   .name = "sensor"},
};

/* Private functions ---------------------------------------------------------*/

static void _set_temperature_sensor( int sensor_number, uint32_t iterator )
{
  LOG( PRINT_INFO, "%s %d", __func__, sensor_number );
}

/* Public functions -----------------------------------------------------------*/

void APITemperatureSensor_Init( void )
{
  JSONParser_RegisterMethod( temperature_sensor_tokens, ARRAY_LEN( temperature_sensor_tokens ), "setTemperatureSensor", NULL, NULL );
}
