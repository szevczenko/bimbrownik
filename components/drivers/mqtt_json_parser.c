/**
 *******************************************************************************
 * @file    mqtt_json_parser.c
 * @author  Dmytro Shevchenko
 * @brief   JSON Parser
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "mqtt_json_parser.h"

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
#define JSON_PARSER_MAX_METHODS 12

/* Private types -------------------------------------------------------------*/

typedef struct
{
  const char* topic;
  json_parse_token_t* tokens;
  mqtt_topic_type_t type;
  size_t tokens_length;
  mqtt_parser_cb init_cb;
  mqtt_parser_get_err_code_cb get_error_code_cb;
  void* user_data;
} json_parse_method_t;

typedef struct
{
  lwjson_token_t tokens[64];
  lwjson_t lwjson;
  json_parse_method_t methods[JSON_PARSER_MAX_METHODS];
  size_t methods_length;
} json_parser_ctx_t;

/* Private variables ---------------------------------------------------------*/
static json_parser_ctx_t ctx;

/* Private functions ---------------------------------------------------------*/

static void _ParseTokensFromMethod( lwjson_token_t* token, json_parse_method_t* method )
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
            method->tokens[i].bool_cb( method->user_data, true );
          }
          break;

        case LWJSON_TYPE_FALSE:
          if ( method->tokens[i].bool_cb != NULL )
          {
            method->tokens[i].bool_cb( method->user_data, false );
          }
          break;

        case LWJSON_TYPE_NUM_INT:
          if ( method->tokens[i].int_cb != NULL )
          {
            method->tokens[i].int_cb( method->user_data, token->u.num_int );
          }
          break;

        case LWJSON_TYPE_NUM_REAL:
          if ( method->tokens[i].double_cb != NULL )
          {
            method->tokens[i].double_cb( method->user_data, token->u.num_real );
          }
          break;

        case LWJSON_TYPE_STRING:
          if ( method->tokens[i].string_cb != NULL )
          {
            method->tokens[i].string_cb( method->user_data, token->u.str.token_value, token->u.str.token_value_len );
          }
          break;

        case LWJSON_TYPE_NULL:
          if ( method->tokens[i].null_cb != NULL )
          {
            method->tokens[i].null_cb( method->user_data );
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
    _ParseTokensFromMethod( token->next, method );
  }
}

typedef struct mqtt_json_parser
{
  const char* name;

} mqtt_type_data_t;

const char* topic_types[] =
  {
    [MQTT_TOPIC_TYPE_SET_CONFIG] = "set/",
    [MQTT_TOPIC_TYPE_GET_CONFIG] = "cfg/",
    [MQTT_TOPIC_TYPE_CONTROL] = "ctl/",
};

static mqtt_topic_type_t _get_topic_type( const char* topic )
{
  for ( mqtt_topic_type_t i = 0; i < MQTT_TOPIC_TYPE_LAST; i++ )
  {
    if ( memcmp( topic_types[i], topic, strlen( topic_types[i] ) ) == 0 )
    {
      return i;
    }
  }
  return MQTT_TOPIC_TYPE_UNKNOWN;
}

/* Public functions ----------------------------------------------------------*/

error_code_t MQTTJsonParse( const char* topic, size_t topic_len, const char* json_string,
                            size_t json_len, char* response, size_t responseLen )
{
  assert( json_string );
  assert( topic );
  mqtt_topic_type_t topic_type = _get_topic_type( topic );
  if ( topic_type == MQTT_TOPIC_TYPE_UNKNOWN )
  {
    return ERROR_CODE_UNKNOWN_MQTT_TOPIC_TYPE;
  }

  uint32_t topic_offset = strlen( topic_types[topic_type] );
  json_parse_method_t* method = NULL;
  for ( int i = 0; i < ARRAY_SIZE( ctx.methods ); i++ )
  {
    if ( ctx.methods[i].type != topic_type )
    {
      continue;
    }
    if ( memcmp( ctx.methods[i].topic, &topic[topic_offset], strlen( ctx.methods[i].topic ) ) != 0 )
    {
      continue;
    }
    uint32_t topic_compare_len = strlen( ctx.methods[i].topic );
    if ( topic_len == topic_offset + topic_compare_len || topic[topic_offset + topic_compare_len] == 0 )
    {
      method = &ctx.methods[i];
      break;
    }
  }

  if ( method == NULL )
  {
    return ERROR_CODE_ERROR_PARSING;
  }

  memset( response, 0, responseLen );
  lwjson_init( &ctx.lwjson, ctx.tokens, LWJSON_ARRAYSIZE( ctx.tokens ) );

  if ( lwjson_parse_ex( &ctx.lwjson, json_string, json_len ) == lwjsonOK )
  {
    lwjson_token_t* t;
    LOG( PRINT_DEBUG, "JSON parsed.." );

    t = lwjson_get_first_token( &ctx.lwjson );
    if ( t->type != LWJSON_TYPE_OBJECT )
    {
      LOG( PRINT_ERROR, "Invalid json" );
      lwjson_free( &ctx.lwjson );
      return ERROR_CODE_ERROR_PARSING;
    }

    for ( lwjson_token_t* tkn = (lwjson_token_t*) lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Token: %.*s", (int) tkn->token_name_len, tkn->token_name );
      _ParseTokensFromMethod( tkn, method );
    }

    lwjson_free( &ctx.lwjson );
  }
  return ERROR_CODE_OK;
}

bool MQTTJsonParser_RegisterMethod( json_parse_token_t* tokens, size_t tokens_length, mqtt_topic_type_t type,
                                    const char* topic, void* user_data, mqtt_parser_cb init_cb,
                                    mqtt_parser_get_err_code_cb get_error_code_cb )
{
  assert( ( topic != NULL ) );
  assert( ( tokens != NULL ) || ( tokens_length == 0 ) );

  if ( ctx.methods_length == JSON_PARSER_MAX_METHODS )
  {
    LOG( PRINT_ERROR, "Methods array is full" );
    return false;
  }
  ctx.methods[ctx.methods_length].topic = topic;
  ctx.methods[ctx.methods_length].type = type;
  ctx.methods[ctx.methods_length].tokens = tokens;
  ctx.methods[ctx.methods_length].tokens_length = tokens_length;
  ctx.methods[ctx.methods_length].init_cb = init_cb;
  ctx.methods[ctx.methods_length].get_error_code_cb = get_error_code_cb;
  ctx.methods[ctx.methods_length].user_data = user_data;
  ctx.methods_length++;
  return true;
}

void MQTTJsonParser_Init( void )
{
  memset( &ctx, 0, sizeof( ctx ) );
}