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
#include "json_parser.h"
#include "mqtt_app.h"
#include "mqtt_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[API MQTT] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define ARRAY_LEN( _array ) sizeof( _array ) / sizeof( _array[0] )

/* Private types -------------------------------------------------------------*/
typedef struct api_mqtt
{
  char buff[512];
  int offset;
  int len;
} cert_block_t;

/* Private functions declaration ---------------------------------------------*/

static void _set_address( const char* str, size_t str_len, uint32_t iterator );
static void _set_ssl( bool value, uint32_t iterator );
static void _set_prefix_topic( const char* str, size_t str_len, uint32_t iterator );
static void _set_post_data_topic( const char* str, size_t str_len, uint32_t iterator );
static void _set_username( const char* str, size_t str_len, uint32_t iterator );
static void _set_password( const char* str, size_t str_len, uint32_t iterator );

static void _offset( int value, uint32_t iterator );
static void _cert_len( int value, uint32_t iterator );
static void _cert( const char* str, size_t str_len, uint32_t iterator );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t mqtt_tokens[] = {
  {.string_cb = _set_address,
   .name = "address"},
  { .bool_cb = _set_ssl,
   .name = "ssl"    },
  { .string_cb = _set_prefix_topic,
   .name = "prefix" },
  { .string_cb = _set_post_data_topic,
   .name = "data"   },
  { .string_cb = _set_username,
   .name = "user"   },
  { .string_cb = _set_password,
   .name = "pass"   },
};

static json_parse_token_t mqtt_cert_tokens[] =
  {
    {.string_cb = _cert,
     .name = "cert"  },
    { .int_cb = _offset,
     .name = "offset"},
};

static json_parse_token_t mqtt_get_cert_tokens[] =
  {
    {.int_cb = _cert_len,
     .name = "len"   },
    { .int_cb = _offset,
     .name = "offset"},
};

static const char* response;
static error_code_t err_code;
static cert_block_t block;

/* Private functions ---------------------------------------------------------*/

static char* str_replace( char* string, const char* substr, const char* replacement )
{
  char* tok = NULL;
  char* newstr = NULL;
  char* oldstr = NULL;

  /* if either substr or replacement is NULL, duplicate string a let caller handle it */

  if ( substr == NULL || replacement == NULL )
  {
    return strdup( string );
  }

  newstr = strdup( string );

  while ( ( tok = strstr( newstr, substr ) ) )
  {
    oldstr = newstr;
    newstr = malloc( strlen( oldstr ) - strlen( substr ) + strlen( replacement ) + 1 );

    /* If failed to alloc mem, free old string and return NULL */
    if ( newstr == NULL )
    {
      free( oldstr );
      return NULL;
    }

    memcpy( newstr, oldstr, tok - oldstr );
    memcpy( newstr + ( tok - oldstr ), replacement, strlen( replacement ) );
    memcpy( newstr + ( tok - oldstr ) + strlen( replacement ), tok + strlen( substr ), strlen( oldstr ) - strlen( substr ) - ( tok - oldstr ) );
    memset( newstr + strlen( oldstr ) - strlen( substr ) + strlen( replacement ), 0, 1 );

    free( oldstr );
  }

  return newstr;
}

static void _init_exec_command( void )
{
  response = NULL;
  err_code = ERROR_CODE_OK;
  memset( &block.buff, 0, sizeof( block.buff ) );
  block.len = -1;
  block.offset = -1;
}

static void _set_error( const char* error_msg )
{
  err_code = ERROR_CODE_FAIL;
  response = error_msg;
}

static error_code_t _get_response( char* resp, size_t respLen )
{
  if ( NULL != response )
  {
    snprintf( resp, respLen, "\"%s\"", response );
  }
  return err_code;
}

static error_code_t _get_cert_response( char* resp, size_t respLen )
{
  if ( block.offset < 0 )
  {
    _set_error( "Cert block len or offset not set" );
    snprintf( resp, respLen, "\"%s\"", response );
    return err_code;
  }

  char* buff = str_replace( block.buff, "\\n", "\n" );

  if ( false == MQTTConfig_SetCert( buff, strlen( buff ), block.offset, MQTT_CONFIG_VALUE_CERT ) )
  {
    _set_error( "Fail set cert block" );
  }

  free( buff );

  if ( NULL != response )
  {
    snprintf( resp, respLen, "\"%s\"", response );
  }
  return err_code;
}

static error_code_t _get_cert( char* resp, size_t respLen )
{
  if ( block.offset < 0 )
  {
    _set_error( "Cert block offset didn't set" );
    snprintf( resp, respLen, "\"%s\"", response );
    return err_code;
  }
  if ( block.len < 0 )
  {
    block.len = sizeof( block.buff );
  }
  const char* cert = MQTTConfig_GetCert( MQTT_CONFIG_VALUE_CERT );
  assert( cert );
  size_t cert_len = strlen( cert ) + 1;
  if ( block.offset > cert_len )
  {
    _set_error( "Offset larger than cert size" );
    snprintf( resp, respLen, "\"%s\"", response );
    return err_code;
  }

  size_t cpy_len = cert_len < block.offset + block.len ? cert_len - block.offset : block.len;
  memcpy( block.buff, &cert[block.offset], cpy_len );
  char* buff = str_replace( block.buff, "\n", "\\n" );
  assert( buff );
  snprintf( resp, respLen, "{\"offset\":%d,\"len\":%d,\"cert_len\":%d,\"cert\":\"%s\"}",
            block.offset, cpy_len, cert_len, buff );
  free( buff );
  return err_code;
}

error_code_t _get_mqtt_config( char* resp, size_t respLen )
{
  const char* address;
  const char* prefix;
  const char* data_topic;
  const char* username;
  const char* password;
  bool ssl;
  address = MQTTConfig_GetString( MQTT_CONFIG_VALUE_ADDRESS );
  if ( NULL == address )
  {
    strncpy( resp, "Fail get address value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == MQTTConfig_GetBool( &ssl, MQTT_CONFIG_VALUE_SSL ) )
  {
    strncpy( resp, "Fail get ssl value", respLen );
    return ERROR_CODE_FAIL;
  }
  prefix = MQTTConfig_GetString( MQTT_CONFIG_VALUE_TOPIC_PREFIX );
  if ( NULL == prefix )
  {
    strncpy( resp, "Fail get prefix value", respLen );
    return ERROR_CODE_FAIL;
  }
  data_topic = MQTTConfig_GetString( MQTT_CONFIG_VALUE_POST_DATA_TOPIC );
  if ( NULL == data_topic )
  {
    strncpy( resp, "Fail get data_topic value", respLen );
    return ERROR_CODE_FAIL;
  }
  username = MQTTConfig_GetString( MQTT_CONFIG_VALUE_USERNAME );
  if ( NULL == username )
  {
    strncpy( resp, "Fail get username value", respLen );
    return ERROR_CODE_FAIL;
  }
  password = MQTTConfig_GetString( MQTT_CONFIG_VALUE_PASSWORD );
  if ( NULL == password )
  {
    strncpy( resp, "Fail get password value", respLen );
    return ERROR_CODE_FAIL;
  }
  snprintf( resp, respLen, "{\"address\":\"%s\",\"ssl\":%s,\"prefix\":\"%s\",\"data\":\"%s\",\"user\":\"%s\",\"pass\":\"%s\"}",
            address, ssl ? "true" : "false", prefix, data_topic, username, password );
  return ERROR_CODE_OK;
}

static void _set_string_value( const char* str, size_t str_len, mqtt_config_value_t value )
{
  if ( str_len >= MQTT_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of value" );
    return;
  }
  char buff[MQTT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == MQTTConfig_SetString( buff, value ) )
  {
    _set_error( "Fail set value" );
  }
}

static error_code_t _mqtt_save_configuration( char* resp, size_t respLen )
{
  if ( MQTTConfig_Save() )
  {
    return ERROR_CODE_OK;
  }
  return ERROR_CODE_FAIL;
}

static void _set_address( const char* str, size_t str_len, uint32_t iterator )
{
  _set_string_value( str, str_len, MQTT_CONFIG_VALUE_ADDRESS );
}

static void _set_ssl( bool value, uint32_t iterator )
{
  if ( false == MQTTConfig_SetBool( value, MQTT_CONFIG_VALUE_SSL ) )
  {
    _set_error( "Fail set tenant value" );
  }
}

static void _set_prefix_topic( const char* str, size_t str_len, uint32_t iterator )
{
  _set_string_value( str, str_len, MQTT_CONFIG_VALUE_TOPIC_PREFIX );
}

static void _set_post_data_topic( const char* str, size_t str_len, uint32_t iterator )
{
  _set_string_value( str, str_len, MQTT_CONFIG_VALUE_POST_DATA_TOPIC );
}

static void _set_username( const char* str, size_t str_len, uint32_t iterator )
{
  _set_string_value( str, str_len, MQTT_CONFIG_VALUE_USERNAME );
}

static void _set_password( const char* str, size_t str_len, uint32_t iterator )
{
  _set_string_value( str, str_len, MQTT_CONFIG_VALUE_PASSWORD );
}

static void _cert( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len <= sizeof( block.buff ) )
  {
    memcpy( block.buff, str, str_len );
  }
  else
  {
    _set_error( "Max size of block cert is 512" );
  }
}

static void _cert_len( int value, uint32_t iterator )
{
  block.len = value;
}

static void _offset( int value, uint32_t iterator )
{
  block.offset = value;
}

/* Public functions -----------------------------------------------------------*/

void API_MQTT_Init( void )
{
  JSONParser_RegisterMethod( mqtt_tokens, ARRAY_LEN( mqtt_tokens ), "setMQTT", _init_exec_command, _get_response );
  JSONParser_RegisterMethod( NULL, 0, "getMQTT", NULL, _get_mqtt_config );
  JSONParser_RegisterMethod( mqtt_cert_tokens, ARRAY_LEN( mqtt_cert_tokens ), "setMQTTCert", _init_exec_command, _get_cert_response );
  JSONParser_RegisterMethod( mqtt_get_cert_tokens, ARRAY_LEN( mqtt_get_cert_tokens ), "getMQTTCert", _init_exec_command, _get_cert );
  JSONParser_RegisterMethod( NULL, 0, "saveMQTT", NULL, _mqtt_save_configuration );
}
