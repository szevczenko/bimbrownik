/**
 *******************************************************************************
 * @file    ota_config.h
 * @author  Dmytro Shevchenko
 * @brief   OTA modules configuration header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _OTA_CONFIG_H
#define _OTA_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "app_events.h"

/* Public macros -------------------------------------------------------------*/

#define OTA_CONFIG_STR_SIZE 64

/* Public types --------------------------------------------------------------*/

typedef enum
{
  OTA_CONFIG_VALUE_ADDRESS,
  OTA_CONFIG_VALUE_TLS,
  OTA_CONFIG_VALUE_TENANT,
  OTA_CONFIG_VALUE_POLLING_TIME,
  OTA_CONFIG_VALUE_TOKEN,
  OTA_CONFIG_VALUE_LAST
} ota_config_value_t;

typedef void ( *ota_apply_config_cb )( void );

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init ota config.
 */
void OTAConfig_Init( void );

bool OTAConfig_SetInt( int value, ota_config_value_t config_value );
bool OTAConfig_SetBool( bool value, ota_config_value_t config_value );
bool OTAConfig_SetString( const char* string, ota_config_value_t config_value );
bool OTAConfig_GetInt( int* value, ota_config_value_t config_value );
bool OTAConfig_GetBool( bool* value, ota_config_value_t config_value );
bool OTAConfig_GetString( char* string, ota_config_value_t config_value, size_t string_len );
bool OTAConfig_Save( void );
void OTAConfig_SetCallback( ota_apply_config_cb cb );

#endif