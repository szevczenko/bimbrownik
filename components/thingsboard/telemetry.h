#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  TELEMETRY_TYPE_STRING,
  TELEMETRY_TYPE_BOOLEAN,
  TELEMETRY_TYPE_DOUBLE,
  TELEMETRY_TYPE_LONG,
  TELEMETRY_TYPE_JSON
} telemetry_key_type_t;

typedef union
{
  char* string_val;
  bool bool_val;
  double double_val;
  long long_val;
  char* json_val;    // JSON string representation
} telemetry_key_data_t;

typedef struct telemetry_key
{
  char* key;
  telemetry_key_type_t type;
  telemetry_key_data_t data;
  struct telemetry_key* next;
} telemetry_key_t;

typedef struct
{
  telemetry_key_t* keys;
  size_t count;
} telemetry_t;

// Function declarations

/**
 * @brief   Create new telemetry object.
 * @return  Pointer to telemetry object - if successful, otherwise NULL
 */
telemetry_t* telemetry_create( void );

/**
 * @brief   Add string key-value pair to telemetry object.
 * @param   [in] tel - Telemetry object pointer.
 * @param   [in] key - Key name for the value.
 * @param   [in] value - String value to add.
 * @return  0 - if successful, otherwise -1
 */
int telemetry_add_string( telemetry_t* tel, const char* key, const char* value );

/**
 * @brief   Add boolean key-value pair to telemetry object.
 * @param   [in] tel - Telemetry object pointer.
 * @param   [in] key - Key name for the value.
 * @param   [in] value - Boolean value to add.
 * @return  0 - if successful, otherwise -1
 */
int telemetry_add_boolean( telemetry_t* tel, const char* key, bool value );

/**
 * @brief   Add double key-value pair to telemetry object.
 * @param   [in] tel - Telemetry object pointer.
 * @param   [in] key - Key name for the value.
 * @param   [in] value - Double value to add.
 * @return  0 - if successful, otherwise -1
 */
int telemetry_add_double( telemetry_t* tel, const char* key, double value );

/**
 * @brief   Add long key-value pair to telemetry object.
 * @param   [in] tel - Telemetry object pointer.
 * @param   [in] key - Key name for the value.
 * @param   [in] value - Long value to add.
 * @return  0 - if successful, otherwise -1
 */
int telemetry_add_long( telemetry_t* tel, const char* key, long value );

/**
 * @brief   Add JSON key-value pair to telemetry object.
 * @param   [in] tel - Telemetry object pointer.
 * @param   [in] key - Key name for the value.
 * @param   [in] json_value - JSON string value to add.
 * @return  0 - if successful, otherwise -1
 */
int telemetry_add_json( telemetry_t* tel, const char* key, const char* json_value );

/**
 * @brief   Convert telemetry object to JSON string.
 * @param   [in] tel - Telemetry object pointer.
 * @return  JSON string - if successful (must be freed), otherwise NULL
 */
char* telemetry_to_json( telemetry_t* tel );

/**
 * @brief   Free telemetry object and all its data.
 * @param   [in] tel - Telemetry object pointer.
 * @return  None
 */
void telemetry_free( telemetry_t* tel );

#ifdef __cplusplus
}
#endif

#endif    // TELEMETRY_H
