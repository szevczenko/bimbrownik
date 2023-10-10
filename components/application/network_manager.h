/**
 *******************************************************************************
 * @file    network_manager.h
 * @author  Dmytro Shevchenko
 * @brief   Network manager module header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _NETWORK_MANAGER_H
#define _NETWORK_MANAGER_H

#include <stdbool.h>

#include "app_events.h"

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/
typedef enum
{
    NETWORK_MANAGER_ERR_OK,
    NETWORK_MANAGER_ERR_TIMEOUT,
    NETWORK_MANAGER_FAIL_OPERATION
}network_manager_err_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init network manager.
 */
void NetworkManagerInit( void );

/**
 * @brief   Sends event to network manager.
 */
void NetworkManagerPostMsg( app_event_t* event );

#endif
