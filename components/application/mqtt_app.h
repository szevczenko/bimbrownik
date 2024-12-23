/**
 *******************************************************************************
 * @file    ota.h
 * @author  Dmytro Shevchenko
 * @brief   MQTT Application layer
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _MQTT_APP_H
#define _MQTT_APP_H

#include <stdbool.h>

#include "app_events.h"

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init mqtt app task.
 */
void MqttApp_Init( void );

/**
 * @brief   Deinit mqtt app task.
 */
void MqttApp_Deinit( void );

/**
 * @brief   Post data.
 */
bool MqttApp_PostData( const char* topic, const char* message, int qos );

#endif