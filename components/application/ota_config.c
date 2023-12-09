/**
 *******************************************************************************
 * @file    ota.c
 * @author  Dmytro Shevchenko
 * @brief   OTA source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "ota_config.h"

#include <string.h>

#include "app_config.h"
#include "json_parser.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Private macros ------------------------------------------------------------*/

#define PARTITION_NAME    "dev_config"
#define STORAGE_NAMESPACE "ota_config"

/* Private types -------------------------------------------------------------*/

typedef enum
{
  VALUE_TYPE_INT,
  VALUE_TYPE_BOOL,
  VALUE_TYPE_STRING
} value_type_t;

typedef struct
{
  const char* name;
  value_type_t type;
  void* value;
  void* default_value;
} value_t;

typedef struct
{
  char address[OTA_CONFIG_STR_SIZE];
  char tenant[OTA_CONFIG_STR_SIZE];
  char token[OTA_CONFIG_STR_SIZE];
  uint8_t use_tls;
  int32_t polling_time;
} config_data_t;

ota_apply_config_cb apply_config_callback = NULL;

/* Private variables ---------------------------------------------------------*/
static config_data_t config_data;

#define _default_address "192.168.1.136:8000"
#define _default_tenant  "DEFAULT"
#define _default_token   "not configured"
static bool default_tls = false;
static int default_poll_time = 300;

static value_t config_values[OTA_CONFIG_VALUE_LAST] =
  {
    [OTA_CONFIG_VALUE_ADDRESS] = {.name = "address",    .type = VALUE_TYPE_STRING, .value = (void*) config_data.address,       .default_value = (void*) _default_address},
    [OTA_CONFIG_VALUE_TLS] = { .name = "tls",       .type = VALUE_TYPE_BOOL,   .value = (void*) &config_data.use_tls,      .default_value = (void*) &default_tls    },
    [OTA_CONFIG_VALUE_TENANT] = { .name = "tenant",    .type = VALUE_TYPE_STRING, .value = (void*) config_data.tenant,        .default_value = (void*) _default_tenant },
    [OTA_CONFIG_VALUE_POLLING_TIME] = { .name = "poll_time", .type = VALUE_TYPE_INT,    .value = (void*) &config_data.polling_time, .default_value = &default_poll_time      },
    [OTA_CONFIG_VALUE_TOKEN] = { .name = "token",     .type = VALUE_TYPE_STRING, .value = (void*) &config_data.token,        .default_value = (void*) _default_token  },
};

static bool _read_data( void )
{
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open_from_partition( PARTITION_NAME, STORAGE_NAMESPACE, NVS_READONLY, &my_handle );
  if ( err != ESP_OK )
  {
    return false;
  }

  for ( int i = 0; i < OTA_CONFIG_VALUE_LAST; i++ )
  {
    switch ( config_values[i].type )
    {
      case VALUE_TYPE_INT:
        err = nvs_get_i32( my_handle, config_values[i].name, config_values[i].value );
        if ( err != ESP_OK )
        {
          memcpy( config_values[i].value, config_values[i].default_value, sizeof( int32_t ) );
        }
        break;

      case VALUE_TYPE_BOOL:
        err = nvs_get_u8( my_handle, config_values[i].name, config_values[i].value );
        if ( err != ESP_OK )
        {
          memcpy( config_values[i].value, config_values[i].default_value, sizeof( uint8_t ) );
        }
        break;

      case VALUE_TYPE_STRING:
        {
          size_t length = OTA_CONFIG_STR_SIZE;
          err = nvs_get_str( my_handle, config_values[i].name, config_values[i].value, &length );
          if ( err != ESP_OK )
          {
            strcpy( config_values[i].value, config_values[i].default_value );
          }
        }
        break;
    }
    if ( err != ESP_OK )
    {
      printf( "[OTA_CONFIG] not found in nvs %s\n\r", config_values[i].name );
    }
  }

  nvs_close( my_handle );
  return true;
}

static bool _save_data( void )
{
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open_from_partition( PARTITION_NAME, STORAGE_NAMESPACE, NVS_READWRITE, &my_handle );
  if ( err != ESP_OK )
  {
    return false;
  }

  for ( int i = 0; i < OTA_CONFIG_VALUE_LAST; i++ )
  {
    switch ( config_values[i].type )
    {
      case VALUE_TYPE_INT:
        err = nvs_set_i32( my_handle, config_values[i].name, *( (int32_t*) config_values[i].value ) );
        break;

      case VALUE_TYPE_BOOL:
        err = nvs_set_u8( my_handle, config_values[i].name, *( (uint8_t*) config_values[i].value ) );
        break;

      case VALUE_TYPE_STRING:
        err = nvs_set_str( my_handle, config_values[i].name, (const char*) config_values[i].value );
        break;
    }
    if ( err != ESP_OK )
    {
      printf( "[OTA_CONFIG] Cannot save %s\n\r", config_values[i].name );
    }
  }

  err = nvs_commit( my_handle );
  if ( err != ESP_OK )
  {
    nvs_close( my_handle );
    return false;
  }
  nvs_close( my_handle );
  return true;
}

static void _set_default_config( void )
{
  for ( int i = 0; i < OTA_CONFIG_VALUE_LAST; i++ )
  {
    switch ( config_values[i].type )
    {
      case VALUE_TYPE_INT:
        memcpy( config_values[i].value, config_values[i].default_value, sizeof( int ) );
        break;

      case VALUE_TYPE_BOOL:
        memcpy( config_values[i].value, config_values[i].default_value, sizeof( bool ) );
        break;

      case VALUE_TYPE_STRING:
        strcpy( config_values[i].value, config_values[i].default_value );
        break;
    }
  }
}

void OTAConfig_Init( void )
{
  if ( false == _read_data() )
  {
    _set_default_config();
  }
}

bool OTAConfig_SetInt( int value, ota_config_value_t config_value )
{
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_INT )
  {
    int* set_value = (int*) config_values[config_value].value;
    *set_value = value;
    return true;
  }
  return false;
}

bool OTAConfig_SetBool( bool value, ota_config_value_t config_value )
{
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_BOOL )
  {
    bool* set_value = (bool*) config_values[config_value].value;
    *set_value = value;
    return true;
  }
  return false;
}

bool OTAConfig_SetString( const char* string, ota_config_value_t config_value )
{
  assert( string );
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_STRING && strlen( string ) < OTA_CONFIG_STR_SIZE )
  {
    char* set_value = (char*) config_values[config_value].value;
    strcpy( set_value, string );
    return true;
  }
  return false;
}

bool OTAConfig_GetInt( int* value, ota_config_value_t config_value )
{
  assert( value );
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_INT )
  {
    *value = *( (int*) config_values[config_value].value );
    return true;
  }
  return false;
}

bool OTAConfig_GetBool( bool* value, ota_config_value_t config_value )
{
  assert( value );
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_BOOL )
  {
    *value = *( (bool*) config_values[config_value].value );
    return true;
  }
  return false;
}

bool OTAConfig_GetString( char* string, ota_config_value_t config_value, size_t string_len )
{
  assert( string && string_len > 0 );
  assert( config_value < OTA_CONFIG_VALUE_LAST );
  if ( config_values[config_value].type == VALUE_TYPE_STRING )
  {
    strncpy( string, (char*) config_values[config_value].value, string_len );
    return true;
  }
  return false;
}

bool OTAConfig_Save( void )
{
  if ( apply_config_callback != NULL )
  {
    apply_config_callback();
  }
  return _save_data();
}

void OTAConfig_SetCallback( ota_apply_config_cb cb )
{
  apply_config_callback = cb;
}
