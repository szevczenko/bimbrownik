/**
 *******************************************************************************
 * @file    api_hawkbit.c
 * @author  Dmytro Shevchenko
 * @brief   API HAWKBIT
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "error_code.h"
#include "hawkbit_config.h"
#include "hawkbit_process.h"
#include "http_server.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[API HAWKBIT] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_LEN( _array ) sizeof( _array ) / sizeof( _array[0] )

#define API_HAWKBIT_URI "/api/hawkbit"

typedef struct
{
  const char* name;
  hawkbit_config_value_t type;
  bool ( *set )( hawkbit_config_value_t key, const char* str, size_t str_len );
  const char* ( *get )( hawkbit_config_value_t key );
} token_t;

/* Private functions declaration ---------------------------------------------*/

static bool set_string_config( hawkbit_config_value_t key, const char* str, size_t str_len );
static const char* get_string_config( hawkbit_config_value_t key );
static bool set_bool_config( hawkbit_config_value_t key, const char* str, size_t str_len );
static const char* get_bool_config( hawkbit_config_value_t key );
static bool set_int_config( hawkbit_config_value_t key, const char* str, size_t str_len );
static const char* get_int_config( hawkbit_config_value_t key );

/* Private variables ---------------------------------------------------------*/

static token_t hawkbit_tokens[] = {
  {.name = "address",   .type = HAWKBIT_CONFIG_VALUE_ADDRESS,      .set = set_string_config, .get = get_string_config},
  {.name = "tenant",    .type = HAWKBIT_CONFIG_VALUE_TENANT,       .set = set_string_config, .get = get_string_config},
  {.name = "tls",       .type = HAWKBIT_CONFIG_VALUE_TLS,          .set = set_bool_config,   .get = get_bool_config  },
  {.name = "poll_time", .type = HAWKBIT_CONFIG_VALUE_POLLING_TIME, .set = set_int_config,    .get = get_int_config   },
  {.name = "token",     .type = HAWKBIT_CONFIG_VALUE_TOKEN,        .set = set_string_config, .get = get_string_config},
};

static const char* response;

/* Private functions ---------------------------------------------------------*/

static int _handle_save_configuration( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method, char* buffer, size_t buffer_size )
{
  snprintf( buffer, buffer_size - 1, "%s/save", API_HAWKBIT_URI );
  struct mg_str save_uri = mg_str( buffer );
  if ( mg_match( *uri, save_uri, NULL ) )
  {
    if ( method != HTTP_SERVER_METHOD_POST )
    {
      response = "Method not allowed";
      return 405;
    }
    if ( HAWKBITConfig_Save() )
    {
      response = "OK";
      return 200;
    }
    else
    {
      response = "Fail to save configuration";
      return 500;
    }
    LOG( PRINT_ERROR, "%s Fail to save configuration", __func__ );
  }
  return 0;
}

static HTTPServerResponse_t _parse_hawkbit_cb( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method )
{
  response = NULL;
  char buffer[128];
  HTTPServerResponse_t resp = { 0 };

  // Handle save configuration
  if ( 0 != _handle_save_configuration( uri, data, method, buffer, sizeof( buffer ) ) )
  {
    resp.msg = "OK";
    resp.code = 200;
    return resp;
  }

  // Handle other configurations
  for ( int i = 0; i < ARRAY_LEN( hawkbit_tokens ); i++ )
  {
    snprintf( buffer, sizeof( buffer ) - 1, "%s/%s", API_HAWKBIT_URI, hawkbit_tokens[i].name );
    struct mg_str parameters_uri = mg_str( buffer );
    if ( mg_match( *uri, parameters_uri, NULL ) )
    {
      switch ( method )
      {
        case HTTP_SERVER_METHOD_GET:
          {
            const char* value = hawkbit_tokens[i].get( hawkbit_tokens[i].type );
            if ( value )
            {
              resp.msg = value;
              resp.code = 200;
            }
            else
            {
              resp.msg = "Fail to get value";
              resp.code = 400;
            }
            return resp;
          }

        case HTTP_SERVER_METHOD_POST:
          assert( data );
          if ( hawkbit_tokens[i].set( hawkbit_tokens[i].type, data->buf, data->len ) )
          {
            resp.msg = "OK";
            resp.code = 200;
          }
          else
          {
            resp.msg = response ? response : "Fail to set value";
            resp.code = 400;
          }
          return resp;

        default:
          resp.msg = "Method not allowed";
          resp.code = 405;
          return resp;
      }
    }
  }
  LOG( PRINT_INFO, "%s %d Parameter not exist %.*s", __func__, uri->len, uri->len, uri->buf );
  resp.msg = "Parameter not exist";
  resp.code = 400;
  return resp;
}

static bool set_string_config( hawkbit_config_value_t key, const char* str, size_t str_len )
{
  if ( str_len >= HAWKBIT_CONFIG_STR_SIZE )
  {
    response = "Invalid size of string";
    return false;
  }
  char buff[HAWKBIT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( !HAWKBITConfig_SetString( buff, key ) )
  {
    response = "Fail set string value";
    return false;
  }
  return true;
}

static const char* get_string_config( hawkbit_config_value_t key )
{
  static char value[HAWKBIT_CONFIG_STR_SIZE];
  if ( !HAWKBITConfig_GetString( value, key, sizeof( value ) ) )
  {
    return '\0';
  }
  return value;
}

static bool set_bool_config( hawkbit_config_value_t key, const char* str, size_t str_len )
{
  bool value = ( strncmp( str, "true", str_len ) == 0 );
  if ( !HAWKBITConfig_SetBool( value, key ) )
  {
    response = "Fail set bool value";
    return false;
  }
  return true;
}

static const char* get_bool_config( hawkbit_config_value_t key )
{
  bool value;
  if ( !HAWKBITConfig_GetBool( &value, key ) )
  {
    return '\0';
  }
  return value ? "true" : "false";
}

static bool set_int_config( hawkbit_config_value_t key, const char* str, size_t str_len )
{
  int value = atoi( str );
  if ( !HAWKBITConfig_SetInt( value, key ) )
  {
    response = "Fail set int value";
    return false;
  }
  return true;
}

static const char* get_int_config( hawkbit_config_value_t key )
{
  static int value;
  static char value_str[16];
  if ( !HAWKBITConfig_GetInt( &value, key ) )
  {
    return '\0';
  }
  snprintf( value_str, sizeof( value_str ), "%d", value );
  return value_str;
}

/* Public functions -----------------------------------------------------------*/

void API_HAWKBIT_Init( void )
{
  HTTPServerApiToken_t token = {
    .api_name = "hawkbit",
    .cb = _parse_hawkbit_cb,
  };

  HTTPServer_AddApiToken( &token );
}
