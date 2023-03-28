#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "unity_fixture.h"

#include "wifidrv.h"
#include "app_manager.h"
#include "network_manager.h"
#include "temperature.h"
#include "config.h"
#include <pthread.h>
#include "lftpd.h"

extern void prvInitialiseHeap( void );


static void RunAllTests(void)
{
}

void _task( void* pv )
{
  lftpd_t lftpd;
	lftpd_start("C", 8080, &lftpd);
  printf("Error, lftpd died\n\r");
  while (1)
  {
    vTaskDelay(1000);
  }
  
}

int main( int argc, const char * argv[] )
{
  // prvInitialiseHeap();
  xTaskCreate( _task, "_task", 1024, NULL, 5, NULL );

  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  AppManagerInit();
  TemperatureInit();
  
  vTaskStartScheduler();
  // return UnityMain(argc, argv, RunAllTests);
}