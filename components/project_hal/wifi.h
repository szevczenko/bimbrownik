/**
 *******************************************************************************
 * @file    wifi.h
 * @author  Dmytro Shevchenko
 * @brief   WiFi Hal Layer
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _WIFI_H
#define _WIFI_H

#include <stdbool.h>
#include <stdint.h>

/* Public types --------------------------------------------------------------*/
typedef struct
{
  char ssid[33];
  char password[64];
} wifiConData_t;

typedef enum
{
  WIFI_ERR_OK,
  WIFI_ERR_INVALID_ARG,
  WIFI_ERR_FAIL,
  WIFI_ERR_LAST
} wifi_err_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init WiFi as access point.
 * @param   [in] name - WiFi ssid name.
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiInitAccessPoint( const char* name );

/**
 * @brief   Init WiFi as STA.
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiInitSTA( void );

/**
 * @brief   Deinit WiFi.
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiDeinit( void );

/**
 * @brief   Connect to access point.
 * @param   [in] ap_name - access point ssid name.
 * @param   [in] password - password.
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiConnect( const char* ap_name, const char* password );

/**
 * @brief   Connect to access point with parameters set before in @c WiFiConnect().
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiConnectLastData( void );

/**
 * @brief   Disconnect from WiFi access point.
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiDisconnect( void );

/**
 * @brief   Stop wifi. Use only when connecting to AP is failed
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiStop( void );

/**
 * @brief   Read info from WiFi adapter
 * @param   [out] rssi - Gets rssi value from adapter
 * @return  error code @c wifi_err_t
 * @note    May be changed in the future for get more parameters
 */
wifi_err_t WiFiReadInfo( int8_t* rssi );

/**
 * @brief   Save connection data in NVS
 * @param   [in] data - Gets rssi value from adapter
 * @return  error code @c wifi_err_t
 */
wifi_err_t wifiDataSave( const wifiConData_t* data );

/**
 * @brief   Read connection data from NVS
 * @param   [out] data - Gets rssi value from adapter
 * @return  error code @c wifi_err_t
 */
wifi_err_t wifiDataRead( wifiConData_t* data );

/**
 * @brief   Start scanning WPS
 * @param   [in] time_ms - time scanning, if time_ms is 0 scan all time
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiWPSStart( uint32_t time_ms );

/**
 * @brief   Stop scanning WPS
 * @return  error code @c wifi_err_t
 */
wifi_err_t WiFiWPSStop( void );

#endif