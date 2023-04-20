/**
 *******************************************************************************
 * @file    tcp_transport.c
 * @author  Dmytro Shevchenko
 * @brief   TCP transport project layer
 *******************************************************************************
 */

#include "tcp_transport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "app_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[TCP] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_TRANSPORT
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define PORT 80

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
  struct sockaddr_in servAddr;
  int optval = 1;
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = INADDR_ANY;
  servAddr.sin_port = htons( PORT );
  // setsockopt( socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof( int ) );
  int rc = bind( socket, (struct sockaddr*) &servAddr, sizeof( struct sockaddr ) );
  if ( rc < 0 )
  {
    LOG( PRINT_ERROR, "bind error %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Listen( int socket )
{
  int rc = listen( socket, 3 );
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
  uint32_t time_wait = 0;
  while ( rc >= 0 )
  {
    int count = 0;
    rc = ioctl( socket, FIONREAD, &count );
    LOG( PRINT_INFO, "Rc %d count: %d", rc, count );
    if ( count > 0 )
    {
      rc = count;
      break;
    }
    vTaskDelay( MS2ST( 5 ) );
    time_wait += 10;
    if ( time_wait > timeout_ms )
    {
      break;
    }
  }

  if ( rc < 0 )
  {
    if ( errno == EINTR )
    {
      rc = 0;
    }
    LOG( PRINT_ERROR, "Select error: %d (%s)", errno, strerror( errno ) );
  }
  return rc;
}

int TCPTransport_Accept( int socket )
{
  struct sockaddr_in servAddr;
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl( INADDR_ANY );
  servAddr.sin_port = htons( PORT );
  socklen_t len = sizeof( servAddr );
  int rc = 0;
  while ( ( rc = accept( socket, (struct sockaddr*) &servAddr, &len ) ) == -1 && errno == EINTR )
    continue;
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
