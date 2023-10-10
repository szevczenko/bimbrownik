/**
 *******************************************************************************
 * @file    app_manager.h
 * @author  Dmytro Shevchenko
 * @brief   App manager modules header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _APP_MANAGER_H
#define _APP_MANAGER_H

#include <stdbool.h>

#include "app_events.h"

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/
typedef enum
{
    APP_MANAGER_ERR_OK,
    APP_MANAGER_ERR_TIMEOUT,
    APP_MANAGER_FAIL_OPERATION
}app_manager_err_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init app manager.
 */
void AppManagerInit( void );

/**
 * @brief   Sends event to app manager.
 */
void AppManagerPostMsg( app_event_t* event );

#endif