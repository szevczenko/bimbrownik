/**
 *******************************************************************************
 * @file    ota.h
 * @author  Dmytro Shevchenko
 * @brief   OTA modules header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _OTA_H
#define _OTA_H

#include <stdbool.h>

#include "app_events.h"

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init ota task.
 */
void OTA_Init( void );

/**
 * @brief   Sends event to ota task.
 */
void OTA_PostMsg( app_event_t* event );

#endif