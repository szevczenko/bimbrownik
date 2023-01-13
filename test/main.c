#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "unity_fixture.h"

#include "wifidrv.h"
#include "app_manager.h"
#include "network_manager.h"
#include "config.h"

extern void prvInitialiseHeap( void );


static void RunAllTests(void)
{
}

void _task( void* pv )
{
  while(1)
  {
    vTaskDelay(200);
  }
}

int main( int argc, const char * argv[] )
{
  prvInitialiseHeap();
  xTaskCreate( _task, "_task", 1024, NULL, 5, NULL );

  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  AppManagerInit();
  
  vTaskStartScheduler();
  // return UnityMain(argc, argv, RunAllTests);
}