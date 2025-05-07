/**
 *******************************************************************************
 * @file    dev_config.c
 * @author  Dmytro Shevchenko
 * @brief   Device config
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "dev_config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEV_CONFIG_SN_SIZE 32

/* Private types -------------------------------------------------------------*/

typedef struct
{
  char serial_number[DEV_CONFIG_SN_SIZE];
} config_data_t;

/* Private variables ---------------------------------------------------------*/
static config_data_t config_data;
static const char* error_lvl_str[] =
  {
    [PRINT_DEBUG] = "DEBUG: ",
    [PRINT_INFO] = "INFO: ",
    [PRINT_WARNING] = "WARNING: ",
    [PRINT_ERROR] = "ERROR: ",
};

void DevConfig_Printf( enum config_print_lvl module_lvl, enum config_print_lvl msg_lvl, const char* format, ... )
{
  if ( module_lvl <= msg_lvl )
  {
    printf( error_lvl_str[msg_lvl] );
    va_list args;
    va_start( args, format );
    vprintf( format, args );
    va_end( args );
    printf( "\n\r" );
  }
}

void DevConfig_Init( void )
{
  sprintf( config_data.serial_number, "SN_TEST_01234567890" );
}

const char* DevConfig_GetSerialNumber( void )
{
  return (const char*) config_data.serial_number;
}

bool DevConfig_SetSerialNumber( const char* sn )
{
  strcpy( config_data.serial_number, sn );
  return true;
}
