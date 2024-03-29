/**
 *******************************************************************************
 * @file    api_device_config.c
 * @author  Dmytro Shevchenko
 * @brief   API device config
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>

#include "app_config.h"
#include "dev_config.h"
#include "esp_app_desc.h"
#include "json_parser.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[API Config] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_LEN( _array ) sizeof( _array ) / sizeof( _array[0] )

/* Private functions ---------------------------------------------------------*/

static error_code_t _get_config( char* response, size_t responseLen )
{
  const esp_app_desc_t* info = esp_app_get_description();
  uint32_t sn = DevConfig_GetSerialNumber();
  snprintf( response, responseLen - 1, "{\"sw\":\"%s\",\"project\":\"AAD\",\"sn\":\"%.6ld\"}", info->version, sn );
  return ERROR_CODE_OK;
}

/* Public functions -----------------------------------------------------------*/

void APIDeviceConfig_Init( void )
{
  JSONParser_RegisterMethod( NULL, 0, "getDeviceConfig", NULL, _get_config );
}
