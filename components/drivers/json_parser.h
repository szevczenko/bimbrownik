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

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/
typedef void ( *method_cb )( void );

typedef struct
{
  const char *name;
  method_cb cb;
} json_parse_token_t;

/* Public functions ----------------------------------------------------------*/

void JSONParse( const char* json_string );

#endif