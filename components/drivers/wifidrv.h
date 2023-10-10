/**
 *******************************************************************************
 * @file    wifi_drv.h
 * @author  Dmytro Shevchenko
 * @brief   WiFi application driver
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _WIFI_DRV_H
#define _WIFI_DRV_H

#include <stdbool.h>

#include "app_events.h"
#include "wifi.h"

/* Public macro --------------------------------------------------------------*/
#define WIFI_AP_NAME "Bimbrownik"

#ifndef WIFI_AP_PASSWORD
#define WIFI_AP_PASSWORD "SuperTrudne1!-_"
#endif

/* Public types --------------------------------------------------------------*/

typedef enum
{
  WIFI_TYPE_ACCESS_POINT,
  WIFI_TYPE_DEVICE,
  WIFI_TYPE_LAST
} wifiType_t;

typedef enum
{
  WIFI_DRV_ERR_OK,
  WIFI_DRV_ERR_CONNECTED,
  WIFI_DRV_ERR_DISCONNECTED,
  WIFI_DRV_ERR_FAIL,
  WIFI_DRV_ERR_LAST
} wifi_drv_err_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init WiFi driver.
 * @param   [in] type - application type @c wifiType_t.
 */
void wifiDrvInit( wifiType_t type );

/**
 * @brief   Gets WiFi RSSI.
 * @return  RSSI value.
 */
int wifiDrvGetRssi( void );

/**
 * @brief   Sends event to WiFi driver.
 */
void WifiDrvPostMsg( app_event_t* event );

#endif