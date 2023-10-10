/**
 *******************************************************************************
 * @file    tcp_transport.h
 * @author  Dmytro Shevchenko
 * @brief   TCP transport layer header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _TCP_TRANSPORT_H_
#define _TCP_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

/* Public functions ----------------------------------------------------------*/

int TCPTransport_CreateSocket( void );
int TCPTransport_Bind( int socket, uint16_t port );
int TCPTransport_Listen( int socket );
int TCPTransport_Select( int socket, uint32_t timeout_ms );
int TCPTransport_Accept( int socket, uint16_t port );
int TCPTransport_Read( int socket, uint8_t* payload, size_t payload_size );
int TCPTransport_Close( int socket );
int TCPTransport_Send( int socket, uint8_t* payload, size_t payload_size );

#endif