/**
 *******************************************************************************
 * @file    wifi_http_app.h
 * @author  Dmytro Shevchenko
 * @brief   Wifi http application
 *******************************************************************************
 */

#ifndef WIFI_HTTP_APP_H_INCLUDED
#define WIFI_HTTP_APP_H_INCLUDED

#include <stdbool.h>

/** 
 * @brief spawns the http server 
 */
void WiFiHTTPApp_Start( void );

/**
 * @brief stops the http server 
 */
void WiFiHTTPApp_Stop( void );

#endif
