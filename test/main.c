#include <pthread.h>

#include "app_manager.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwjson.h"
#include "network_manager.h"
#include "temperature.h"
#include "unity_fixture.h"
#include "wifidrv.h"
#include "json_parser.h"
#include "tcp_server.h"

extern void prvInitialiseHeap( void );

static void RunAllTests( void )
{
}

void _task( void* pv )
{
  printf( "Error, lftpd died\n\r" );
  while ( 1 )
  {
    vTaskDelay( 1000 );
  }
}

int main( int argc, const char* argv[] )
{
  // prvInitialiseHeap();
  xTaskCreate( _task, "_task", 1024, NULL, 5, NULL );

  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  AppManagerInit();
  TemperatureInit();
  TCPServer_Init();

  vTaskStartScheduler();
  // return UnityMain(argc, argv, RunAllTests);
}