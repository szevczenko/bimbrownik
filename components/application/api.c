/**
 *******************************************************************************
 * @file    api.c
 * @author  Dmytro Shevchenko
 * @brief   Communication API
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "app_config.h"
#include "json_parser.h"

/* Extern functions ---------------------------------------------------------*/
extern void APITemperatureSensor_Init( void );
extern void APIDeviceConfig_Init( void );
extern void API_OTA_Init( void );

/* Public functions -----------------------------------------------------------*/

void API_Init( void )
{
  APITemperatureSensor_Init();
  APIDeviceConfig_Init();
  API_OTA_Init();
}
