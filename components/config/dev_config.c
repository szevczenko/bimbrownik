/**
 *******************************************************************************
 * @file    dev_config.c
 * @author  Dmytro Shevchenko
 * @brief   Device config
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "dev_config.h"

#include <string.h>

#include "app_config.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Private macros ------------------------------------------------------------*/

#define PARTITION_NAME    "dev_config"
#define STORAGE_NAMESPACE "config"

/* Private types -------------------------------------------------------------*/

typedef struct
{
  uint32_t serial_number;
} config_data_t;

/* Private variables ---------------------------------------------------------*/
static config_data_t config_data;

static bool _read_data( void )
{
  nvs_handle_t my_handle;
  esp_err_t err;

  err = nvs_open_from_partition( PARTITION_NAME, STORAGE_NAMESPACE, NVS_READONLY, &my_handle );
  if ( err != ESP_OK )
  {
    printf( "Error read " STORAGE_NAMESPACE " %d\n\r", err );
    return false;
  }

  err = nvs_get_u32( my_handle, "SN", (void*) &config_data.serial_number );

  nvs_close( my_handle );
  if ( err != ESP_OK )
  {
    printf( "Error nvs_get_u32 %d\n\r", err );
    return false;
  }
  return true;
}

void DevConfig_Init( void )
{
  nvs_flash_init_partition(PARTITION_NAME);
  if ( false == _read_data() )
  {
    printf( "[DEVICE CONFIG] Critical error: cannot read device config\n\r" );
  }
}

uint32_t DevConfig_GetSerialNumber( void )
{
  return config_data.serial_number;
}
