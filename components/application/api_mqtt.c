/**
 *******************************************************************************
 * @file    api_mqtt.c
 * @author  Dmytro Shevchenko
 * @brief   API MQTT
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "error_code.h"
#include "http_server.h"
#include "mqtt_app.h"
#include "mqtt_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[API MQTT] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_HTTP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_LEN( _array ) sizeof( _array ) / sizeof( _array[0] )

#define API_MQTT_URI "/api/mqtt"

typedef struct
{
  const char* name;
  mqtt_config_value_t type;
  bool ( *set )( mqtt_config_value_t key, const char* str, size_t str_len );
  const char* ( *get )( mqtt_config_value_t key );
} token_t;

/* Private functions declaration ---------------------------------------------*/

static bool set_string_config( mqtt_config_value_t key, const char* str, size_t str_len );
static const char* get_string_config( mqtt_config_value_t key );
static bool set_bool_config( mqtt_config_value_t key, const char* str, size_t str_len );
static const char* get_bool_config( mqtt_config_value_t key );
static bool set_cert_config( mqtt_config_value_t key, const char* str, size_t str_len );
static const char* get_cert_config( mqtt_config_value_t key );

/* Private variables ---------------------------------------------------------*/

static token_t mqtt_tokens[] = {
  {.name = "address", .type = MQTT_CONFIG_VALUE_ADDRESS,         .set = set_string_config, .get = get_string_config},
  {.name = "ssl",     .type = MQTT_CONFIG_VALUE_SSL,             .set = set_bool_config,   .get = get_bool_config  },
  {.name = "prefix",  .type = MQTT_CONFIG_VALUE_TOPIC_PREFIX,    .set = set_string_config, .get = get_string_config},
  {.name = "data",    .type = MQTT_CONFIG_VALUE_POST_DATA_TOPIC, .set = set_string_config, .get = get_string_config},
  {.name = "user",    .type = MQTT_CONFIG_VALUE_USERNAME,        .set = set_string_config, .get = get_string_config},
  {.name = "pass",    .type = MQTT_CONFIG_VALUE_PASSWORD,        .set = set_string_config, .get = get_string_config},
  {.name = "cert",    .type = MQTT_CONFIG_VALUE_CERT,            .set = set_cert_config,   .get = get_cert_config  },
};

static const char* response;

/* Private functions ---------------------------------------------------------*/

static int _handle_save_configuration( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method, char* buffer, size_t buffer_size )
{
  snprintf( buffer, buffer_size - 1, "%s/save", API_MQTT_URI );
  struct mg_str save_uri = mg_str( buffer );
  if ( mg_match( *uri, save_uri, NULL ) )
  {
    LOG(PRINT_INFO, "Save configuration");
    if ( method != HTTP_SERVER_METHOD_POST )
    {
      LOG(PRINT_INFO, "Method not allowed %d", method);
      response = "Method not allowed";
      return 405;
    }
    if ( MQTTConfig_Save() )
    {
      LOG(PRINT_INFO, "Save success");
      response = "OK";
      return 200;
    }
    else
    {
      LOG( PRINT_ERROR, "%s Fail to save configuration", __func__ );
      response = "Fail to save configuration";
      return 500;
    }
  }
  return 0;
}

static HTTPServerResponse_t _parse_mqtt_cb( struct mg_str* uri, struct mg_str* data, HTTPServerMethod_t method )
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
  for ( int i = 0; i < ARRAY_LEN( mqtt_tokens ); i++ )
  {
    snprintf( buffer, sizeof( buffer ) - 1, "%s/%s", API_MQTT_URI, mqtt_tokens[i].name );
    struct mg_str parameters_uri = mg_str( buffer );
    if ( mg_match( *uri, parameters_uri, NULL ) )
    {
      switch ( method )
      {
        case HTTP_SERVER_METHOD_GET:
          {
            const char* value = mqtt_tokens[i].get( mqtt_tokens[i].type );
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
          if ( mqtt_tokens[i].set( mqtt_tokens[i].type, data->buf, data->len ) )
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

static bool set_string_config( mqtt_config_value_t key, const char* str, size_t str_len )
{
  if ( str_len >= MQTT_CONFIG_STR_SIZE )
  {
    response = "Invalid size of string";
    return false;
  }
  char buff[MQTT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( !MQTTConfig_SetString( buff, key ) )
  {
    response = "Fail set string value";
    return false;
  }
  return true;
}

static const char* get_string_config( mqtt_config_value_t key )
{
  return MQTTConfig_GetString( key );
}

static bool set_bool_config( mqtt_config_value_t key, const char* str, size_t str_len )
{
  bool value = ( strncmp( str, "true", str_len ) == 0 );
  if ( !MQTTConfig_SetBool( value, key ) )
  {
    response = "Fail set bool value";
    return false;
  }
  return true;
}

static const char* get_bool_config( mqtt_config_value_t key )
{
  bool value;
  if ( !MQTTConfig_GetBool( &value, key ) )
  {
    return '\0';
  }
  return value ? "true" : "false";
}

static bool set_cert_config( mqtt_config_value_t key, const char* str, size_t str_len )
{
  if ( str_len >= MQTT_CERT_MAX_SIZE )
  {
    response = "Invalid size of certificate";
    return false;
  }
  if ( !MQTTConfig_SetCert( str, str_len, 0, key ) )
  {
    response = "Fail set certificate value";
    return false;
  }
  return true;
}

static const char* get_cert_config( mqtt_config_value_t key )
{
  return MQTTConfig_GetCert( key );
}

/* Public functions -----------------------------------------------------------*/

void API_MQTT_Init( void )
{
  HTTPServerApiToken_t token = {
    .api_name = "mqtt",
    .cb = _parse_mqtt_cb,
  };

  HTTPServer_AddApiToken( &token );
}
