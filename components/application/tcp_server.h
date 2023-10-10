/**
 *******************************************************************************
 * @file    cmd_server.h
 * @author  Dmytro Shevchenko
 * @brief   TCP server for receive data from client header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _CMD_SERVER_H
#define _CMD_SERVER_H

#include <stddef.h>
#include <stdint.h>

#include "app_events.h"

/* Public macro --------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

void TCPServer_Init( void );
void TCPServer_PostMsg( app_event_t* event );
int TCPServer_SendData( uint8_t* buff, size_t len );

#endif