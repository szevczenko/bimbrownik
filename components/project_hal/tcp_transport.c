/**
 *******************************************************************************
 * @file    tcp_transport.c
 * @author  Dmytro Shevchenko
 * @brief   TCP transport project layer
 *******************************************************************************
 */

#include "tcp_transport.h"

#include <errno.h>
#include <lwip/def.h>
#include <lwip/sockets.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "app_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[TCP] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_CLIENT
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define PORT 1234

/* Private functions ---------------------------------------------------------*/

int TCPTransport_CreateSocket( void )
{
  int rc = socket( AF_INET, SOCK_STREAM, 0 );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Create socket failed: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Bind( int socket )
{
  struct sockaddr_in servaddr = {};
  int optval = 1;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
  servaddr.sin_port = htons( PORT );
  setsockopt( socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( int ) );
  int rc = bind( socket, (struct sockaddr*) &servaddr, sizeof( servaddr ) );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "bind error %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Listen( int socket )
{
  int rc = listen( socket, 5 );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Listen error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Select( int socket, uint32_t timeout_ms )
{
  int rc = 0;
  fd_set set;

  FD_ZERO( &set );
  FD_SET( socket, &set );
  struct timeval timeout_time = {};
  timeout_time.tv_sec = timeout_ms / 1000;
  timeout_time.tv_usec = ( timeout_ms % 1000 ) * 1000;
  rc = select( socket + 1, &set, NULL, NULL, &timeout_time );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Select error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Accept( int socket )
{
  struct sockaddr_in servaddr = {};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
  servaddr.sin_port = htons( PORT );
  socklen_t len = sizeof( servaddr );
  int rc = accept( socket, (struct sockaddr*) &servaddr, &len );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Accept error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Read( int socket, uint8_t* payload, size_t payload_size )
{
  int rc = read( socket, (char*) payload, payload_size );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Read error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Close( int socket )
{
  if ( socket != -1 )
  {
    close( socket );
  }
  return 0;
}

int TCPTransport_Send( int socket, uint8_t* payload, size_t payload_size )
{
  int rc = send( socket, payload, payload_size, 0 );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "Send error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}
