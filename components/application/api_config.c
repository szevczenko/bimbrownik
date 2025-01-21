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
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
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
static TimerHandle_t restart_timer;

/* Private functions ---------------------------------------------------------*/

static void restart_timer_callback( TimerHandle_t xTimer )
{
  esp_restart();
}

static bool set_serial_number( const char* str, size_t str_len )
{
  char sn[DEV_CONFIG_MAX_STRING_LEN] = {};
  char magic_word[DEV_CONFIG_MAX_STRING_LEN] = {};

  struct mg_str json = mg_str_n( str, str_len );
  char* sn_token = mg_json_get_str( json, "$.sn" );
  char* magic_token = mg_json_get_str( json, "$.magic" );

  printf( "JSON: %.*s\n", (int) json.len, json.buf );
  printf( "Serial number: %s\n", sn_token );
  printf( "Magic word: %s\n", magic_token );

  if ( !sn_token || strlen( sn_token ) >= DEV_CONFIG_MAX_STRING_LEN )
  {
    snprintf( response_buffer, sizeof( response_buffer ), "Invalid size of serial number" );
    free( sn_token );
    free( magic_token );
    return false;
  }
  strncpy( sn, sn_token, DEV_CONFIG_MAX_STRING_LEN - 1 );

  if ( magic_token && strlen( magic_token ) > 0 )
  {
    if ( strlen( magic_token ) >= DEV_CONFIG_MAX_STRING_LEN )
    {
      snprintf( response_buffer, sizeof( response_buffer ), "Invalid size of magic word" );
      free( sn_token );
      free( magic_token );
      return false;
    }
    strncpy( magic_word, magic_token, DEV_CONFIG_MAX_STRING_LEN - 1 );
  }

  const char* current_sn = DevConfig_GetSerialNumber();
  if ( current_sn && strlen( current_sn ) > 0 && strcmp( magic_word, "SUPER_GRASS" ) != 0 )
  {
    snprintf( response_buffer, sizeof( response_buffer ), "Serial number already set" );
    free( sn_token );
    free( magic_token );
    return false;
  }

  if ( !DevConfig_SetSerialNumber( sn ) )
  {
    snprintf( response_buffer, sizeof( response_buffer ), "Fail to set serial number" );
    free( sn_token );
    free( magic_token );
    return false;
  }

  free( sn_token );
  free( magic_token );
  return true;
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
          xTimerStart( restart_timer, 0 );
          response.code = 200;
          sprintf( response_buffer, "OK" );
        }
        else if ( mg_match( *uri, mg_str( "/api/dev_config/serial_number" ), NULL ) )
        {
          if ( set_serial_number( data->buf, data->len ) )
          {
            response.code = 200;
            sprintf( response_buffer, "OK" );
          }
          else
          {
            printf( "Failed to set serial number: %s\n", response_buffer );
            response.code = 400;
          }
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

  restart_timer = xTimerCreate( "RestartTimer", pdMS_TO_TICKS( 5000 ), pdFALSE, (void*) 0, restart_timer_callback );
}
