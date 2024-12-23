/**
 *******************************************************************************
 * @file    api_device_config.c
 * @author  Dmytro Shevchenko
 * @brief   API device config
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <ctype.h>
#include <string.h>

#include "app_config.h"
#include "dev_config.h"
#include "esp_app_desc.h"
#include "http_server.h"

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

/* Private variables ---------------------------------------------------------*/

static char response_buffer[128];

/* Private functions ---------------------------------------------------------*/

static HTTPServerResponse_t _config_parse_cb( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method )
{
  HTTPServerResponse_t response = { .msg = response_buffer };

  switch ( method )
  {
    case HTTP_SERVER_METHOD_GET:
      {
        const esp_app_desc_t* info = esp_app_get_description();
        uint32_t sn = DevConfig_GetSerialNumber();
        snprintf( response_buffer, sizeof( response_buffer ) - 1, "{\"sw\":\"%s\",\"project\":\"AAD\",\"sn\":\"%.6ld\"}", info->version, sn );
        response.code = 200;
        return response;
      }

    default:
      sprintf( response_buffer, "Method not allowed" );
      response.code = 405;
      return response;
  }
  return response;
}

/* Public functions ---------------------------------------------------------*/

void APIDeviceConfig_Init( void )
{
  HTTPServerApiToken_t token = {
    .api_name = "deviceConfig",
    .cb = _config_parse_cb,
  };

  HTTPServer_AddApiToken( &token );
}
