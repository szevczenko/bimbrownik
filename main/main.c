#include <stdbool.h>
#include <stdio.h>

#include "app_config.h"
#include "app_events.h"
#include "app_manager.h"
#include "dev_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_app.h"
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
#include "device_manager.h"

static const char* TAG = "MAIN";

void fs_init( void )
{
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true };

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register( &conf );

  if ( ret != ESP_OK )
  {
    if ( ret == ESP_FAIL )
    {
      ESP_LOGE( TAG, "Failed to mount or format filesystem" );
    }
    else if ( ret == ESP_ERR_NOT_FOUND )
    {
      ESP_LOGE( TAG, "Failed to find SPIFFS partition" );
    }
    else
    {
      ESP_LOGE( TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name( ret ) );
    }
    return;
  }

  ESP_LOGI( TAG, "Performing SPIFFS_check()." );
  ret = esp_spiffs_check( conf.partition_label );
  if ( ret != ESP_OK )
  {
    ESP_LOGE( TAG, "SPIFFS_check() failed (%s)", esp_err_to_name( ret ) );
    return;
  }
  else
  {
    ESP_LOGI( TAG, "SPIFFS_check() successful" );
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info( conf.partition_label, &total, &used );
  if ( ret != ESP_OK )
  {
    ESP_LOGE( TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name( ret ) );
    esp_spiffs_format( conf.partition_label );
    return;
  }
  else
  {
    ESP_LOGI( TAG, "Partition size: total: %d, used: %d", total, used );
  }

  // Check consistency of reported partition size info.
  if ( used > total )
  {
    ESP_LOGW( TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check()." );
    ret = esp_spiffs_check( conf.partition_label );
    // Could be also used to mend broken files, to clean unreferenced pages, etc.
    // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
    if ( ret != ESP_OK )
    {
      ESP_LOGE( TAG, "SPIFFS_check() failed (%s)", esp_err_to_name( ret ) );
      return;
    }
    else
    {
      ESP_LOGI( TAG, "SPIFFS_check() successful" );
    }
  }
}

void app_init( void )
{
  nvs_flash_init();
  nvs_sync_create();
  fs_init();
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
  MQTTApp_Init();
  DeviceManager_Init();
  // screenInit();

  AppManagerInit();
}
