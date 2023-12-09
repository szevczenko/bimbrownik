#include <stdbool.h>
#include <stdio.h>

#include "app_config.h"
#include "app_events.h"
#include "app_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "nvs_flash.h"
#include "nvs_sync.h"
#include "ota.h"
#include "ow/devices/ow_device_ds18x20.h"
#include "ow/ow.h"
#include "ow_esp32.h"
#include "screen.h"
#include "tcp_server.h"
#include "temperature.h"
#include "wifidrv.h"
#include "dev_config.h"

void app_init( void )
{
  nvs_flash_init();
  nvs_sync_create();
  DevConfig_Init();
}

void app_main( void )
{
  app_init();

  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  TemperatureInit();
  TCPServer_Init();
  OTA_Init();
  // screenInit();

  AppManagerInit();
}
