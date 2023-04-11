/**
 *******************************************************************************
 * @file    temperature.h
 * @author  Dmytro Shevchenko
 * @brief   Temperature application driver
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _TEMP_DRV_H
#define _TEMP_DRV_H

#include <stdbool.h>

#include "app_events.h"

/* Public macro --------------------------------------------------------------*/
#define TEMP_NUMBER_OF_SENSORS

/* Public types --------------------------------------------------------------*/

typedef enum
{
  TEMP_DRV_ERR_OK,
  TEMP_DRV_ERR_FAIL,
  TEMP_DRV_ERR_LAST
} temp_drv_err_t;

typedef struct
{
  temp_drv_err_t err;
  uint32_t rom[TEMP_NUMBER_OF_SENSORS];
}temp_drv_scan_response;


/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init temperature measure driver.
 */
void TemperatureInit( void );

/**
 * @brief   Sends event to temperature driver.
 */
void TemperaturePostMsg( app_event_t* event );

#endif