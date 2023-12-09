/**
 *******************************************************************************
 * @file    api_ota.c
 * @author  Dmytro Shevchenko
 * @brief   API OTA
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "json_parser.h"
#include "ota.h"
#include "ota_config.h"

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

/* Private functions declaration ---------------------------------------------*/

static void _set_address( const char* str, size_t str_len, uint32_t iterator );
static void _set_tenant( const char* str, size_t str_len, uint32_t iterator );
static void _set_tls( bool value, uint32_t iterator );
static void _set_polling_time( int value, uint32_t iterator );
static void _set_token( const char* str, size_t str_len, uint32_t iterator );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t ota_tokens[] = {
  {.string_cb = _set_address,
   .name = "address"  },
  { .string_cb = _set_tenant,
   .name = "tenant"   },
  { .bool_cb = _set_tls,
   .name = "tls"      },
  { .int_cb = _set_polling_time,
   .name = "poll_time"},
   { .string_cb = _set_token,
   .name = "token"   },
};

static const char* response;
static error_code_t err_code;

/* Private functions ---------------------------------------------------------*/

static void _init_exec_command( void )
{
  response = NULL;
  err_code = ERROR_CODE_OK;
}

error_code_t _get_response( char* resp, size_t respLen )
{
  if (NULL != response)
  {
    strncpy( resp, response, respLen );
  }
  return err_code;
}

static void _set_error( const char* error_msg )
{
  err_code = ERROR_CODE_FAIL;
  response = error_msg;
}

error_code_t _get_ota_config( char* resp, size_t respLen )
{
  static char address[OTA_CONFIG_STR_SIZE];
  static char tenant[OTA_CONFIG_STR_SIZE];
  static char token[OTA_CONFIG_STR_SIZE];
  bool use_tls;
  int polling_time;

  if ( false == OTAConfig_GetString( address, OTA_CONFIG_VALUE_ADDRESS, sizeof( address ) ) )
  {
    strncpy( resp, "Fail get address value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == OTAConfig_GetString( tenant, OTA_CONFIG_VALUE_TENANT, sizeof( tenant ) ) )
  {
    strncpy( resp, "Fail get tenant value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == OTAConfig_GetString( token, OTA_CONFIG_VALUE_TOKEN, sizeof( token ) ) )
  {
    strncpy( resp, "Fail get token value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == OTAConfig_GetBool( &use_tls, OTA_CONFIG_VALUE_TLS ) )
  {
    strncpy( resp, "Fail get tls value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == OTAConfig_GetInt( &polling_time, OTA_CONFIG_VALUE_POLLING_TIME ) )
  {
    strncpy( resp, "Fail get polling time value", respLen );
    return ERROR_CODE_FAIL;
  }
  snprintf( resp, respLen, "{\"address\":\"%s\",\"tenant\":\"%s\",\"tls\":%s,\"poll_time\":%d,\"token\":\"%s\"}",
            address, tenant, use_tls ? "true" : "false", polling_time, token );
  return ERROR_CODE_OK;
}

error_code_t _ota_save_configuration( char* resp, size_t respLen )
{
  if ( OTAConfig_Save() )
  {
    return ERROR_CODE_OK;
  }
  return ERROR_CODE_FAIL;
}

static void _set_address( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= OTA_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of address" );
    return;
  }
  char buff[OTA_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == OTAConfig_SetString( buff, OTA_CONFIG_VALUE_ADDRESS ) )
  {
    _set_error( "Fail set address value" );
  }
}

static void _set_token( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= OTA_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of address" );
    return;
  }
  char buff[OTA_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == OTAConfig_SetString( buff, OTA_CONFIG_VALUE_TOKEN ) )
  {
    _set_error( "Fail set token value" );
  }
}

static void _set_tenant( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= OTA_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of tenant" );
    return;
  }
  char buff[OTA_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == OTAConfig_SetString( buff, OTA_CONFIG_VALUE_TENANT ) )
  {
    _set_error( "Fail set tenant value" );
  }
}

static void _set_tls( bool value, uint32_t iterator )
{
  if ( false == OTAConfig_SetBool( value, OTA_CONFIG_VALUE_TLS ) )
  {
    _set_error( "Fail set tenant value" );
  }
}

static void _set_polling_time( int value, uint32_t iterator )
{
  if ( false == OTAConfig_SetInt( value, OTA_CONFIG_VALUE_POLLING_TIME ) )
  {
    _set_error( "Fail set polling time value" );
  }
}

/* Public functions -----------------------------------------------------------*/

void API_OTA_Init( void )
{
  JSONParser_RegisterMethod( ota_tokens, ARRAY_LEN( ota_tokens ), "setOTA", _init_exec_command, _get_response );
  JSONParser_RegisterMethod( NULL, 0, "getOTA", NULL, _get_ota_config );
  JSONParser_RegisterMethod( NULL, 0, "saveOTA", NULL, _ota_save_configuration );
}
