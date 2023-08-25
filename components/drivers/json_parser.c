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

static json_parse_method_t* _GetMethod( lwjson_token_t* token )
{
  if ( token->type != LWJSON_TYPE_OBJECT )
  {
    return NULL;
  }
  for ( size_t i = 0; i < ctx.methods_length; i++ )
  {
    if ( 0 == strncmp( ctx.methods[i].name, token->token_name, token->token_name_len ) )
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

static bool _ParseTokensFromMethod( lwjson_token_t* token, json_parse_method_t* method, uint32_t iterator )
{
  bool result = false;
  for ( size_t i = 0; i < method->tokens_length; i++ )
  {
    if ( strncmp( token->token_name, method->tokens[i].name, token->token_name_len ) == 0 )
    {
      switch ( token->type )
      {
        case LWJSON_TYPE_TRUE:
          if ( method->tokens[i].bool_cb != NULL )
          {
            result = method->tokens[i].bool_cb( true, iterator );
          }
          break;

        case LWJSON_TYPE_FALSE:
          if ( method->tokens[i].bool_cb != NULL )
          {
            result = method->tokens[i].bool_cb( false, iterator );
          }
          break;

        case LWJSON_TYPE_NUM_INT:
          if ( method->tokens[i].int_cb != NULL )
          {
            result = method->tokens[i].int_cb( token->u.num_int, iterator );
          }
          break;

        case LWJSON_TYPE_NUM_REAL:
          if ( method->tokens[i].double_cb != NULL )
          {
            result = method->tokens[i].double_cb( token->u.num_real, iterator );
          }
          break;

        case LWJSON_TYPE_STRING:
          if ( method->tokens[i].string_cb != NULL )
          {
            result = method->tokens[i].string_cb( token->u.str.token_value, token->u.str.token_value_len, iterator );
          }
          break;

        case LWJSON_TYPE_NULL:
          if ( method->tokens[i].null_cb != NULL )
          {
            result = method->tokens[i].null_cb( iterator );
          }
          break;

        default:
          break;
      }
      break;
    }
  }
  if ( result == false )
  {
    LOG( PRINT_WARNING, "Don't found token: %.*s in method %s", token->token_name_len, token->token_name, method->name );
  }
  if ( token->next != NULL )
  {
    result |= _ParseTokensFromMethod( token->next, method, iterator );
  }
  return result;
}

/* Public functions ----------------------------------------------------------*/

bool JSONParse( const char* json_string )
{
  assert( json_string );
  bool result = false;
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
      return false;
    }

    /* Now print all keys in the object */
    json_parse_method_t* method = NULL;
    lwjson_token_t* method_token = NULL;
    uint32_t iterator = 0;
    bool is_iterator_read = false;
    for ( lwjson_token_t* tkn = lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Token: %.*s", (int) tkn->token_name_len, tkn->token_name );
      if ( method == NULL )
      {
        method = _GetMethod( tkn );
        if ( method != NULL )
        {
          method_token = lwjson_get_first_child( tkn );
        }
        continue;
      }
      if ( is_iterator_read == false )
      {
        if ( is_iterator_read = _GetIterator( tkn, &iterator ) )
        {
          continue;
        }
      }
    }

    if ( method != NULL && method_token != NULL )
    {
      LOG( PRINT_DEBUG, "Child token name: %s", method_token->token_name );
      result = _ParseTokensFromMethod( method_token, method, iterator );
    }

    /* Call this when not used anymore */
    lwjson_free( &ctx.lwjson );
  }
  return result;
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