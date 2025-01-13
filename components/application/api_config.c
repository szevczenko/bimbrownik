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
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

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
static TimerHandle_t restart_timer;

/* Private functions ---------------------------------------------------------*/

static void restart_timer_callback(TimerHandle_t xTimer)
{
    esp_restart();
}

static HTTPServerResponse_t _config_parse_cb( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method )
{
  HTTPServerResponse_t response = { .msg = response_buffer };

  switch ( method )
  {
    case HTTP_SERVER_METHOD_GET:
      {
        const esp_app_desc_t* info = esp_app_get_description();
        const char* sn = DevConfig_GetSerialNumber();
        snprintf( response_buffer, sizeof( response_buffer ) - 1, "{\"sw\":\"%s\",\"project\":\"AAD\",\"sn\":\"%s\"}", info->version, sn );
        response.code = 200;
        return response;
      }
    case HTTP_SERVER_METHOD_POST:
      {
        if ( mg_match( *uri, mg_str( "/api/dev_config/restart" ), NULL ) )
        {
          xTimerStart(restart_timer, 0);
          response.code = 200;
          sprintf( response_buffer, "OK" );
        }
        else
        {
          response.code = 404;
          sprintf( response_buffer, "Not found" );
        }
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
    .api_name = "dev_config",
    .cb = _config_parse_cb,
  };

  HTTPServer_AddApiToken( &token );

  restart_timer = xTimerCreate("RestartTimer", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, restart_timer_callback);
}
