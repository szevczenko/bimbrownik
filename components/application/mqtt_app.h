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
void MQTTApp_Init( void );

/**
 * @brief   Sends event to mqtt app task.
 */
void MQTTApp_PostMsg( app_event_t* event );

#endif