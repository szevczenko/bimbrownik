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
#include "json_parser.h"
#include "hawkbit_process.h"
#include "hawkbit_config.h"

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

/* Private functions declaration ---------------------------------------------*/

static void _set_address( const char* str, size_t str_len, uint32_t iterator );
static void _set_tenant( const char* str, size_t str_len, uint32_t iterator );
static void _set_tls( bool value, uint32_t iterator );
static void _set_polling_time( int value, uint32_t iterator );
static void _set_token( const char* str, size_t str_len, uint32_t iterator );

/* Private variables ---------------------------------------------------------*/

static json_parse_token_t hawkbit_tokens[] = {
  {.string_cb = _set_address,
   .name = "address"  },
  { .string_cb = _set_tenant,
   .name = "tenant"   },
  { .bool_cb = _set_tls,
   .name = "tls"      },
  { .int_cb = _set_polling_time,
   .name = "poll_time"},
  { .string_cb = _set_token,
   .name = "token"    },
};

static const char* response;
static error_code_t err_code;

/* Private functions ---------------------------------------------------------*/

static void _init_exec_command( void )
{
  response = NULL;
  err_code = ERROR_CODE_OK;
}

static error_code_t _get_response( char* resp, size_t respLen )
{
  if ( NULL != response )
  {
    snprintf( resp, respLen, "\"%s\"", response );
  }
  return err_code;
}

static void _set_error( const char* error_msg )
{
  err_code = ERROR_CODE_FAIL;
  response = error_msg;
}

error_code_t _get_hawkbit_config( char* resp, size_t respLen )
{
  static char address[HAWKBIT_CONFIG_STR_SIZE];
  static char tenant[HAWKBIT_CONFIG_STR_SIZE];
  static char token[HAWKBIT_CONFIG_STR_SIZE];
  bool use_tls;
  int polling_time;

  if ( false == HAWKBITConfig_GetString( address, HAWKBIT_CONFIG_VALUE_ADDRESS, sizeof( address ) ) )
  {
    strncpy( resp, "Fail get address value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == HAWKBITConfig_GetString( tenant, HAWKBIT_CONFIG_VALUE_TENANT, sizeof( tenant ) ) )
  {
    strncpy( resp, "Fail get tenant value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == HAWKBITConfig_GetString( token, HAWKBIT_CONFIG_VALUE_TOKEN, sizeof( token ) ) )
  {
    strncpy( resp, "Fail get token value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == HAWKBITConfig_GetBool( &use_tls, HAWKBIT_CONFIG_VALUE_TLS ) )
  {
    strncpy( resp, "Fail get tls value", respLen );
    return ERROR_CODE_FAIL;
  }
  if ( false == HAWKBITConfig_GetInt( &polling_time, HAWKBIT_CONFIG_VALUE_POLLING_TIME ) )
  {
    strncpy( resp, "Fail get polling time value", respLen );
    return ERROR_CODE_FAIL;
  }
  snprintf( resp, respLen, "{\"address\":\"%s\",\"tenant\":\"%s\",\"tls\":%s,\"poll_time\":%d,\"token\":\"%s\"}",
            address, tenant, use_tls ? "true" : "false", polling_time, token );
  return ERROR_CODE_OK;
}

error_code_t _hawkbit_save_configuration( char* resp, size_t respLen )
{
  if ( HAWKBITConfig_Save() )
  {
    return ERROR_CODE_OK;
  }
  return ERROR_CODE_FAIL;
}

static void _set_address( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= HAWKBIT_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of address" );
    return;
  }
  char buff[HAWKBIT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == HAWKBITConfig_SetString( buff, HAWKBIT_CONFIG_VALUE_ADDRESS ) )
  {
    _set_error( "Fail set address value" );
  }
}

static void _set_token( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= HAWKBIT_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of address" );
    return;
  }
  char buff[HAWKBIT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == HAWKBITConfig_SetString( buff, HAWKBIT_CONFIG_VALUE_TOKEN ) )
  {
    _set_error( "Fail set token value" );
  }
}

static void _set_tenant( const char* str, size_t str_len, uint32_t iterator )
{
  if ( str_len >= HAWKBIT_CONFIG_STR_SIZE )
  {
    _set_error( "Invalid size of tenant" );
    return;
  }
  char buff[HAWKBIT_CONFIG_STR_SIZE] = {};
  memcpy( buff, str, str_len );
  if ( false == HAWKBITConfig_SetString( buff, HAWKBIT_CONFIG_VALUE_TENANT ) )
  {
    _set_error( "Fail set tenant value" );
  }
}

static void _set_tls( bool value, uint32_t iterator )
{
  if ( false == HAWKBITConfig_SetBool( value, HAWKBIT_CONFIG_VALUE_TLS ) )
  {
    _set_error( "Fail set tenant value" );
  }
}

static void _set_polling_time( int value, uint32_t iterator )
{
  if ( false == HAWKBITConfig_SetInt( value, HAWKBIT_CONFIG_VALUE_POLLING_TIME ) )
  {
    _set_error( "Fail set polling time value" );
  }
}

/* Public functions -----------------------------------------------------------*/

void API_HAWKBIT_Init( void )
{
  JSONParser_RegisterMethod( hawkbit_tokens, ARRAY_LEN( hawkbit_tokens ), "setHawkbit", _init_exec_command, _get_response );
  JSONParser_RegisterMethod( NULL, 0, "getHawkbit", NULL, _get_hawkbit_config );
  JSONParser_RegisterMethod( NULL, 0, "saveHawkbit", NULL, _hawkbit_save_configuration );
}
