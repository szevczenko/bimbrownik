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

json_parse_method_t* _GetMethod( lwjson_token_t* token )
{
  for ( size_t i = 0; i < ctx.methods_length; i++ )
  {
    if ( ( token->type == LWJSON_TYPE_OBJECT ) && ( 0 == strncmp( ctx.methods[i].name, token->token_name, token->token_name_len ) ) )
    {
      return &ctx.methods[i];
    }
  }
  LOG( PRINT_ERROR, "Don't found method: %.*s", token->token_name_len, token->token_name );
  return NULL;
}

bool _ParseTokensFromMethod( lwjson_token_t* token, json_parse_method_t* method )
{
  bool result = false;
  for ( size_t i = 0; i < method->tokens_length; i++ )
  {
    if ( strncmp( token->token_name, method->tokens[i].name, token->token_name_len ) == 0 )
    {
      method->tokens[i].cb();
      result = true;
      break;
    }
  }
  if ( result == false )
  {
    LOG( PRINT_WARNING, "Don't found token: %.*s in method %s", token->token_name_len, token->token_name, method->name );
  }
  if ( token->next != NULL )
  {
    result |= _ParseTokensFromMethod( token->next, method );
  }
  return result;
}

/* Public functions ----------------------------------------------------------*/

void JSONParse( const char* json_string )
{
  /* Initialize and pass statically allocated tokens */
  lwjson_init( &ctx.lwjson, ctx.tokens, LWJSON_ARRAYSIZE( ctx.tokens ) );

  /* Try to parse input string */
  if ( lwjson_parse( &ctx.lwjson, json_string ) == lwjsonOK )
  {
    lwjson_token_t* t;
    LOG( PRINT_INFO, "JSON parsed.." );

    /* Get very first token as top object */
    t = lwjson_get_first_token( &ctx.lwjson );
    if ( t->type != LWJSON_TYPE_OBJECT )
    {
      LOG( PRINT_ERROR, "Invalid json" );
      lwjson_free( &ctx.lwjson );
      return;
    }

    /* Now print all keys in the object */
    for ( const lwjson_token_t* tkn = lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Token: %.*s", (int) tkn->token_name_len, tkn->token_name );
      json_parse_method_t* method = _GetMethod( tkn );
      if ( method != NULL )
      {
        lwjson_token_t* child_token = lwjson_get_first_child( tkn );
        LOG( PRINT_DEBUG, "Child token name: %s", child_token->token_name );
        _ParseTokensFromMethod( child_token, method );
      }
    }

    /* Call this when not used anymore */
    lwjson_free( &ctx.lwjson );
  }
}

bool JSONParser_RegisterMethod( json_parse_token_t* tokens, size_t tokens_length, const char* method_name )
{
  assert( ( tokens != NULL ) && ( tokens_length != 0 ) && ( method_name != NULL ) );
  if ( ctx.methods_length == JSON_PARSER_MAX_METHODS )
  {
    LOG( PRINT_ERROR, "Methods array is full" );
    return false;
  }
  ctx.methods[ctx.methods_length].name = method_name;
  ctx.methods[ctx.methods_length].tokens = tokens;
  ctx.methods[ctx.methods_length].tokens_length = tokens_length;
  ctx.methods_length++;
  return true;
}