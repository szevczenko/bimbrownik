/**
 *******************************************************************************
 * @file    wifidrv.c
 * @author  Dmytro Shevchenko
 * @brief   Wifi Drv
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include <inttypes.h>
#include <string.h>

#include "app_config.h"
#include "app_events.h"
#include "esp_efuse.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[OTA] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CONFIG_TCPIP_EVENT_THD_WA_SIZE 4096
#define OTA_URL_SIZE                   256

/* Extern variables ----------------------------------------------------------*/
extern const uint8_t server_cert_pem_start[] asm( "_binary_ca_cert_pem_start" );
extern const uint8_t server_cert_pem_end[] asm( "_binary_ca_cert_pem_end" );

/* Private functions ---------------------------------------------------------*/
static void _event_handler( void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data )
{
  if ( event_base == ESP_HTTPS_OTA_EVENT )
  {
    switch ( event_id )
    {
      case ESP_HTTPS_OTA_START:
        LOG( PRINT_INFO, "OTA started" );
        break;
      case ESP_HTTPS_OTA_CONNECTED:
        LOG( PRINT_INFO, "Connected to server" );
        break;
      case ESP_HTTPS_OTA_GET_IMG_DESC:
        LOG( PRINT_INFO, "Reading Image Description" );
        break;
      case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
        LOG( PRINT_INFO, "Verifying chip id of new image: %d", *(esp_chip_id_t*) event_data );
        break;
      case ESP_HTTPS_OTA_DECRYPT_CB:
        LOG( PRINT_INFO, "Callback to decrypt function" );
        break;
      case ESP_HTTPS_OTA_WRITE_FLASH:
        LOG( PRINT_DEBUG, "Writing to flash: %d written", *(int*) event_data );
        break;
      case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
        LOG( PRINT_INFO, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t*) event_data );
        break;
      case ESP_HTTPS_OTA_FINISH:
        LOG( PRINT_INFO, "OTA finish" );
        break;
      case ESP_HTTPS_OTA_ABORT:
        LOG( PRINT_INFO, "OTA abort" );
        break;
    }
  }
}

static esp_err_t _validate_image_header( esp_app_desc_t* new_app_info )
{
  if ( new_app_info == NULL )
  {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_app_desc_t running_app_info;
  if ( esp_ota_get_partition_description( running, &running_app_info ) == ESP_OK )
  {
    LOG( PRINT_INFO, "Running firmware version: %s", running_app_info.version );
  }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
  if ( memcmp( new_app_info->version, running_app_info.version, sizeof( new_app_info->version ) ) == 0 )
  {
    LOG( PRINT_WARNING, "Current running version is the same as a new. We will not continue the update." );
    return ESP_FAIL;
  }
#endif

  const uint32_t hw_sec_version = esp_efuse_read_secure_version();
  if ( new_app_info->secure_version < hw_sec_version )
  {
    LOG( PRINT_WARNING, "New firmware security version is less than eFuse programmed, %" PRIu32 " < %" PRIu32, new_app_info->secure_version, hw_sec_version );
    return ESP_FAIL;
  }

  return ESP_OK;
}

static esp_err_t _http_client_init_cb( esp_http_client_handle_t http_client )
{
  esp_err_t err = ESP_OK;
  /* Uncomment to add custom headers to HTTP request */
  // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
  return err;
}

static void _ota_task( void* pvParameter )
{
  LOG( PRINT_INFO, "Starting Advanced OTA example" );

  esp_err_t ota_finish_err = ESP_OK;
  esp_http_client_config_t config = {
    .url = "https://192.168.1.155:8001/main.bin",
    .cert_pem = (char*) server_cert_pem_start,
    .timeout_ms = 3000,
    .keep_alive_enable = true,
    .skip_cert_common_name_check = true,
  };

  esp_https_ota_config_t ota_config = {
    .http_config = &config,
    .http_client_init_cb = _http_client_init_cb,    // Register a callback to be invoked after esp_http_client is initialized
    .partial_http_download = true,
    .max_http_request_size = 8 * 1024,
  };

  esp_https_ota_handle_t https_ota_handle = NULL;
  esp_err_t err = esp_https_ota_begin( &ota_config, &https_ota_handle );
  if ( err != ESP_OK )
  {
    LOG( PRINT_ERROR, "ESP HTTPS OTA Begin failed" );
    vTaskDelete( NULL );
  }

  esp_app_desc_t app_desc;
  err = esp_https_ota_get_img_desc( https_ota_handle, &app_desc );
  if ( err != ESP_OK )
  {
    LOG( PRINT_ERROR, "esp_https_ota_read_img_desc failed" );
    goto ota_end;
  }
  err = _validate_image_header( &app_desc );
  if ( err != ESP_OK )
  {
    LOG( PRINT_ERROR, "image header verification failed" );
    goto ota_end;
  }

  while ( 1 )
  {
    err = esp_https_ota_perform( https_ota_handle );
    if ( err != ESP_ERR_HTTPS_OTA_IN_PROGRESS )
    {
      break;
    }
    // esp_https_ota_perform returns after every read operation which gives user the ability to
    // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
    // data read so far.
    LOG( PRINT_DEBUG, "Image bytes read: %d", esp_https_ota_get_image_len_read( https_ota_handle ) );
  }

  if ( esp_https_ota_is_complete_data_received( https_ota_handle ) != true )
  {
    // the OTA image was not completely received and user can customise the response to this situation.
    LOG( PRINT_ERROR, "Complete data was not received." );
  }
  else
  {
    ota_finish_err = esp_https_ota_finish( https_ota_handle );
    if ( ( err == ESP_OK ) && ( ota_finish_err == ESP_OK ) )
    {
      LOG( PRINT_INFO, "ESP_HTTPS_OTA upgrade successful. Rebooting ..." );
      vTaskDelay( 1000 / portTICK_PERIOD_MS );
      esp_restart();
    }
    else
    {
      if ( ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED )
      {
        LOG( PRINT_ERROR, "Image validation failed, image is corrupted" );
      }
      LOG( PRINT_ERROR, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err );
      vTaskDelete( NULL );
    }
  }

ota_end:
  esp_https_ota_abort( https_ota_handle );
  LOG( PRINT_ERROR, "ESP_HTTPS_OTA upgrade failed" );
  vTaskDelete( NULL );
}

void OTA_Init( void )
{
  esp_event_handler_register( ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &_event_handler, NULL );

  /**
     * We are treating successful WiFi connection as a checkpoint to cancel rollback
     * process and mark newly updated firmware image as active. For production cases,
     * please tune the checkpoint behavior per end application requirement.
     */
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if ( esp_ota_get_state_partition( running, &ota_state ) == ESP_OK )
  {
    if ( ota_state == ESP_OTA_IMG_PENDING_VERIFY )
    {
      if ( esp_ota_mark_app_valid_cancel_rollback() == ESP_OK )
      {
        LOG( PRINT_INFO, "App is valid, rollback cancelled successfully" );
      }
      else
      {
        LOG( PRINT_ERROR, "Failed to cancel rollback" );
      }
    }
  }
  xTaskCreate( &_ota_task, "_ota_task", 1024 * 8, NULL, 5, NULL );
}
