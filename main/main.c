#include <stdbool.h>
#include <stdio.h>

#include "app_events.h"
#include "app_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "wifidrv.h"

void app_main( void )
{
  printf( "Hello World\n\r" );
  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  AppManagerInit();
}
