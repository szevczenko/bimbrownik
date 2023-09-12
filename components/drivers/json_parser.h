/**
 *******************************************************************************
 * @file    json_parser.h
 * @author  Dmytro Shevchenko
 * @brief   JSON parser
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _JSON_PARSER_H_
#define _JSON_PARSER_H_

#include <stdbool.h>
#include <stddef.h>

#include "error_code.h"
#include "lwjson.h"

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/
typedef void ( *json_parser_cb )( void );
typedef error_code_t ( *json_parser_get_err_code_cb )( char* response, size_t responseLen );
typedef void ( *method_bool_cb )( bool value, uint32_t iterator );
typedef void ( *method_int_cb )( int value, uint32_t iterator );
typedef void ( *method_null_cb )( uint32_t iterator );
typedef void ( *method_double_cb )( double value, uint32_t iterator );
typedef void ( *method_string_cb )( const char* str, size_t str_len, uint32_t iterator );

typedef struct
{
  const char* name;
  method_bool_cb bool_cb;
  method_int_cb int_cb;
  method_null_cb null_cb;
  method_double_cb double_cb;
  method_string_cb string_cb;
} json_parse_token_t;

/* Public functions ----------------------------------------------------------*/

error_code_t JSONParse( const char* json_string, size_t jsonLen, uint32_t* iterator, char *response, size_t responseLen );

bool JSONParser_RegisterMethod( json_parse_token_t* tokens, size_t tokens_length, const char* method_name, json_parser_cb init_cb, json_parser_get_err_code_cb get_error_code_cb );

void JSONParser_Init( void );

#endif