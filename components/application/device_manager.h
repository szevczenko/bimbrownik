/**
 *******************************************************************************
 * @file    device_manager.h
 * @author  Dmytro Shevchenko
 * @brief   Device manager header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_

#include <stdbool.h>

#include "app_events.h"

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init mqtt app task.
 */
void DeviceManager_Init( void );

/**
 * @brief   Sends event to mqtt app task.
 */
void DeviceManager_PostMsg( app_event_t* event );

#endif