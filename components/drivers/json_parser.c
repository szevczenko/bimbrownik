/**
 *******************************************************************************
 * @file    json_parser.c
 * @author  Dmytro Shevchenko
 * @brief   JSON Parser
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "json_parser.h"

#include <string.h>

#include "app_config.h"
#include "lwjson.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[JSON] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_JSON
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define METHOD_NAME_MAX_SIZE    32
#define ARRAY_SIZE( _array )    sizeof( _array ) / sizeof( _array[0] )
#define JSON_PARSER_MAX_METHODS 8

/* Private types -------------------------------------------------------------*/

typedef struct
{
  const char* name;
  json_parse_token_t* tokens;
  size_t tokens_length;
  json_parser_cb init_cb;
  json_parser_get_err_code_cb get_error_code_cb;
} json_parse_method_t;

typedef struct
{
  lwjson_token_t tokens[128];
  lwjson_t lwjson;
  json_parse_method_t methods[JSON_PARSER_MAX_METHODS];
  size_t methods_length;
} json_parser_ctx_t;

/* Private variables ---------------------------------------------------------*/
json_parser_ctx_t ctx;

/* Private functions ---------------------------------------------------------*/

static json_parse_method_t* _GetMethod( lwjson_token_t* token, lwjson_token_t** method_token_p )
{
  if ( token->type != LWJSON_TYPE_STRING && ( 0 == strncmp( "method", token->token_name, token->token_name_len ) ) )
  {
    return NULL;
  }
  *method_token_p = token;
  size_t str_len = 0;
  const char* method_read = lwjson_get_val_string( token, &str_len );
  for ( size_t i = 0; i < ctx.methods_length; i++ )
  {
    if ( 0 == strncmp( ctx.methods[i].name, method_read, str_len ) )
    {
      return &ctx.methods[i];
    }
  }
  LOG( PRINT_ERROR, "Don't found method: %.*s", token->token_name_len, token->token_name );
  return NULL;
}

static bool _GetIterator( lwjson_token_t* token, uint32_t* iterator )
{
  if ( ( token->type == LWJSON_TYPE_NUM_INT ) && ( 0 == strncmp( "i", token->token_name, token->token_name_len ) ) )
  {
    *iterator = lwjson_get_val_int( token );
    return true;
  }
  return false;
}

static bool _isDataToken( lwjson_token_t* token )
{
  if ( ( token->type == LWJSON_TYPE_OBJECT ) && ( 0 == strncmp( "data", token->token_name, token->token_name_len ) ) )
  {
    return true;
  }
  return false;
}

static void _ParseTokensFromMethod( lwjson_token_t* token, json_parse_method_t* method, uint32_t iterator )
{
  for ( size_t i = 0; i < method->tokens_length; i++ )
  {
    if ( strncmp( token->token_name, method->tokens[i].name, token->token_name_len ) == 0 )
    {
      switch ( token->type )
      {
        case LWJSON_TYPE_TRUE:
          if ( method->tokens[i].bool_cb != NULL )
          {
            method->tokens[i].bool_cb( true, iterator );
          }
          break;

        case LWJSON_TYPE_FALSE:
          if ( method->tokens[i].bool_cb != NULL )
          {
            method->tokens[i].bool_cb( false, iterator );
          }
          break;

        case LWJSON_TYPE_NUM_INT:
          if ( method->tokens[i].int_cb != NULL )
          {
            method->tokens[i].int_cb( token->u.num_int, iterator );
          }
          break;

        case LWJSON_TYPE_NUM_REAL:
          if ( method->tokens[i].double_cb != NULL )
          {
            method->tokens[i].double_cb( token->u.num_real, iterator );
          }
          break;

        case LWJSON_TYPE_STRING:
          if ( method->tokens[i].string_cb != NULL )
          {
            method->tokens[i].string_cb( token->u.str.token_value, token->u.str.token_value_len, iterator );
          }
          break;

        case LWJSON_TYPE_NULL:
          if ( method->tokens[i].null_cb != NULL )
          {
            method->tokens[i].null_cb( iterator );
          }
          break;

        default:
          break;
      }
      break;
    }
  }
  if ( token->next != NULL )
  {
    _ParseTokensFromMethod( token->next, method, iterator );
  }
}

/* Public functions ----------------------------------------------------------*/

error_code_t JSONParse( const char* json_string, size_t jsonLen, uint32_t* iterator, char* response, size_t responseLen )
{
  assert( json_string );
  assert( iterator );
  error_code_t error_code = ERROR_CODE_ERROR_PARSING;
  memset( response, 0, responseLen );
  lwjson_init( &ctx.lwjson, ctx.tokens, LWJSON_ARRAYSIZE( ctx.tokens ) );

  if ( lwjson_parse_ex( &ctx.lwjson, json_string, jsonLen ) == lwjsonOK )
  {
    lwjson_token_t* t;
    LOG( PRINT_INFO, "JSON parsed.." );

    /* Get very first token as top object */
    t = lwjson_get_first_token( &ctx.lwjson );
    if ( t->type != LWJSON_TYPE_OBJECT )
    {
      LOG( PRINT_ERROR, "Invalid json" );
      lwjson_free( &ctx.lwjson );
      return error_code;
    }

    /* Now print all keys in the object */
    json_parse_method_t* method = NULL;
    lwjson_token_t* method_token = NULL;
    lwjson_token_t* data_token = NULL;
    bool is_iterator_read = false;
    *iterator = 0;
    for ( lwjson_token_t* tkn = (lwjson_token_t*) lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Token: %.*s", (int) tkn->token_name_len, tkn->token_name );
      if ( method_token == NULL )
      {
        method = _GetMethod( tkn, &method_token );
        continue;
      }
      if ( is_iterator_read == false )
      {
        is_iterator_read = _GetIterator( tkn, iterator );
        if ( is_iterator_read )
        {
          continue;
        }
      }
      if ( data_token == NULL )
      {
        if ( _isDataToken( tkn ) )
        {
          data_token = (lwjson_token_t*) lwjson_get_first_child( tkn );
        }
      }
    }

    if ( method != NULL )
    {
      if ( method->init_cb != NULL )
      {
        method->init_cb();
      }
      if ( data_token != NULL )
      {
        _ParseTokensFromMethod( data_token, method, *iterator );
      }

      if ( method->get_error_code_cb != NULL )
      {
        error_code = method->get_error_code_cb( response, responseLen );
      }
      else
      {
        error_code = ERROR_CODE_OK_NO_ACK;
        strncpy( response, ErrorCode_GetStr( error_code ), responseLen - 1 );
      }
    }
    lwjson_free( &ctx.lwjson );
  }
  return error_code;
}

bool JSONParser_RegisterMethod( json_parse_token_t* tokens, size_t tokens_length, const char* method_name, json_parser_cb init_cb, json_parser_get_err_code_cb get_error_code_cb )
{
  assert( ( method_name != NULL ) );
  assert( ( tokens != NULL ) || ( tokens_length == 0 ) );

  if ( ctx.methods_length == JSON_PARSER_MAX_METHODS )
  {
    LOG( PRINT_ERROR, "Methods array is full" );
    return false;
  }
  ctx.methods[ctx.methods_length].name = method_name;
  ctx.methods[ctx.methods_length].tokens = tokens;
  ctx.methods[ctx.methods_length].tokens_length = tokens_length;
  ctx.methods[ctx.methods_length].init_cb = init_cb;
  ctx.methods[ctx.methods_length].get_error_code_cb = get_error_code_cb;
  ctx.methods_length++;
  return true;
}

void JSONParser_Init( void )
{
  memset( &ctx, 0, sizeof( ctx ) );
}