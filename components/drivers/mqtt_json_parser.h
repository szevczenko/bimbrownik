/**
 *******************************************************************************
 * @file    mqtt_parser.h
 * @author  Dmytro Shevchenko
 * @brief   MQTT JSON parser
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _MQTT_JSON_PARSER_H_
#define _MQTT_JSON_PARSER_H_

#include <stdbool.h>
#include <stddef.h>

#include "error_code.h"
#include "lwjson.h"

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/
typedef void ( *mqtt_parser_cb )( void );
typedef error_code_t ( *mqtt_parser_get_err_code_cb )( char* response, size_t responseLen );
typedef void ( *method_bool_cb )( void* user_data, bool value );
typedef void ( *method_int_cb )( void* user_data, int value );
typedef void ( *method_null_cb )( void* user_data );
typedef void ( *method_double_cb )( void* user_data, double value );
typedef void ( *method_string_cb )( void* user_data, const char* str, size_t str_len );

typedef struct
{
  const char* name;
  method_bool_cb bool_cb;
  method_int_cb int_cb;
  method_null_cb null_cb;
  method_double_cb double_cb;
  method_string_cb string_cb;
} json_parse_token_t;

typedef enum
{
  MQTT_TOPIC_TYPE_SET_CONFIG,
  MQTT_TOPIC_TYPE_GET_CONFIG,
  MQTT_TOPIC_TYPE_CONTROL,
  MQTT_TOPIC_TYPE_LAST,
  MQTT_TOPIC_TYPE_UNKNOWN
} mqtt_topic_type_t;

/* Public functions ----------------------------------------------------------*/

error_code_t MQTTJsonParse( const char* topic, size_t topic_len, const char* json_string,
                            size_t json_len, char* response, size_t responseLen );

bool MQTTJsonParser_RegisterMethod( json_parse_token_t* tokens, size_t tokens_length, mqtt_topic_type_t type,
                                    const char* topic, void* user_data, mqtt_parser_cb init_cb,
                                    mqtt_parser_get_err_code_cb get_error_code_cb );

void MQTTJsonParser_Init( void );

#endif