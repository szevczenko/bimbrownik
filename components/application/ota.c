/**
 *******************************************************************************
 * @file    ota.c
 * @author  Dmytro Shevchenko
 * @brief   OTA source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "ota.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "dev_config.h"
#include "esp_crt_bundle.h"
#include "esp_efuse.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota_config.h"
#include "ota_parser.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[OTA] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Extern variables ----------------------------------------------------------*/
extern const uint8_t server_cert_pem_start[] asm( "_binary_ca_cert_pem_start" );
extern const uint8_t server_cert_pem_end[] asm( "_binary_ca_cert_pem_end" );

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                        \
  STATE( DISABLED, _disabled_state_handler_array ) \
  STATE( IDLE, _idle_state_handler_array )         \
  STATE( DOWNLOADED, _downloaded_state_handler_array )

typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} module_state_t;

typedef enum
{
  OTA_UPDATE_RESULT_NONE,
  OTA_UPDATE_RESULT_SUCCESS,
  OTA_UPDATE_RESULT_FAILED
} ota_update_result_t;

typedef struct

{
  module_state_t state;
  QueueHandle_t queue;
  ota_update_result_t ota_update_result;
  char update_result_details[256];
  char action_id[32];

  int file_size;
  int downloaded_size;
  uint8_t download_percent;

  char url[256];
  bool use_tls;
  char address[OTA_CONFIG_STR_SIZE];
  char tenant[OTA_CONFIG_STR_SIZE];

} module_ctx_t;

typedef enum
{
  TIMER_ID_POLLING,
  TIMER_ID_LAST
} timer_id;

/* Private functions declaration ---------------------------------------------*/
static void _timer_polling( TimerHandle_t xTimer );

static void _state_disabled_init( const app_event_t* event );

static void _state_idle_event_polling( const app_event_t* event );
static void _state_idle_event_post_config_data( const app_event_t* event );
static void _state_idle_event_ota_download_image( const app_event_t* event );
static void _state_idle_event_post_ota_result( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_init ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_OTA_POLL_SERVER, _state_idle_event_polling ),
    EVENT_ITEM( MSG_ID_OTA_POST_CONFIG_DATA, _state_idle_event_post_config_data ),
    EVENT_ITEM( MSG_ID_OTA_DOWNLOAD_IMAGE, _state_idle_event_ota_download_image ),
    EVENT_ITEM( MSG_ID_OTA_POST_OTA_RESULT, _state_idle_event_post_ota_result ),
};

static const struct app_events_handler _downloaded_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_idle_event_polling ),
};

/* Private variables ---------------------------------------------------------*/
static char localResponseBuffer[2048];
static char urlConfigData[OTA_URL_SIZE];
static char urlDeploymentBase[OTA_URL_SIZE];
static char urlDeploymentBaseFeedback[OTA_URL_SIZE + 16];
static ota_deployment_t otaDeployment;

static module_ctx_t ctx;

struct state_context
{
  const module_state_t state;
  const char* name;
  const struct app_events_handler* event_handler_array;
  uint8_t event_handler_array_size;
};

static const struct state_context module_state[STATE_TOP] =
  {
#define STATE( _state, _event_handler_array )    \
  {                                              \
    .state = _state,                             \
    .name = #_state,                             \
    .event_handler_array = _event_handler_array, \
    .event_handler_array_size = ARRAY_SIZE( _event_handler_array ) },
    STATE_HANDLER_ARRAY
#undef STATE
};

static app_timer_t timers[] =
  {
    TIMER_ITEM( TIMER_ID_POLLING, _timer_polling, 300000, "AppTimeoutInit" ),
};

/* Private functions ---------------------------------------------------------*/

static const char* _get_state_name( const module_state_t state )
{
  return module_state[state].name;
}

static void _change_state( module_state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_OTA, APP_EVENT_OTA );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_OTA, APP_EVENT_OTA, data, data_size );
  }

  OTA_PostMsg( &event );
}

esp_err_t ota_bundle_attach( void* conf )
{
  mbedtls_ssl_config* ssl_conf = (mbedtls_ssl_config*) conf;
  if ( ssl_conf != NULL )
  {
    mbedtls_ssl_conf_authmode( ssl_conf, MBEDTLS_SSL_VERIFY_OPTIONAL );
  }
  return esp_crt_bundle_attach( ssl_conf );
}

bool _find_action_id( const char* str, char* id, size_t id_len )
{
  memset( id, 0, id_len );
  const char* string_search = "deploymentBase/";
  char* tmp = strstr( str, string_search );

  if ( tmp == NULL )
  {
    return false;
  }

  bool result = false;
  tmp += strlen( string_search );
  for ( int i = 0; i < id_len - 1; i++ )
  {
    if ( tmp[i] == '/' || tmp[i] == 0 || tmp[i] == '?' )
    {
      result = true;
      break;
    }
    id[i] = tmp[i];
  }
  printf( id );
  return result;
}

static void _timer_polling( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_OTA_POLL_SERVER, NULL, 0 );
}

esp_err_t _http_event_handler( esp_http_client_event_t* evt )
{
  static char* output_buffer;    // Buffer to store response of http request from event handler
  static int output_len;    // Stores number of bytes read
  switch ( evt->event_id )
  {
    case HTTP_EVENT_ON_HEADER:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value );
      break;
    case HTTP_EVENT_ON_DATA:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len );
      if ( !esp_http_client_is_chunked_response( evt->client ) )
      {
        // If user_data buffer is configured, copy the response into the buffer
        int copy_len = 0;
        if ( evt->user_data )
        {
          copy_len = MIN( evt->data_len, ( sizeof( localResponseBuffer ) - output_len ) );
          if ( copy_len )
          {
            memcpy( evt->user_data + output_len, evt->data, copy_len );
          }
        }
        else
        {
          const int buffer_len = esp_http_client_get_content_length( evt->client );
          if ( output_buffer == NULL )
          {
            output_buffer = (char*) malloc( buffer_len );
            output_len = 0;
            if ( output_buffer == NULL )
            {
              LOG( PRINT_ERROR, "Failed to allocate memory for output buffer" );
              return ESP_FAIL;
            }
          }
          copy_len = MIN( evt->data_len, ( buffer_len - output_len ) );
          if ( copy_len )
          {
            memcpy( output_buffer + output_len, evt->data, copy_len );
          }
        }
        output_len += copy_len;
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_FINISH" );
      if ( output_buffer != NULL )
      {
        free( output_buffer );
        output_buffer = NULL;
      }
      output_len = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      LOG( PRINT_INFO, "HTTP_EVENT_DISCONNECTED" );
      int mbedtls_err = 0;
      esp_err_t err = esp_tls_get_and_clear_last_error( (esp_tls_error_handle_t) evt->data, &mbedtls_err, NULL );
      if ( err != 0 )
      {
        LOG( PRINT_INFO, "Last esp error code: 0x%x", err );
        LOG( PRINT_INFO, "Last mbedtls failure: 0x%x", mbedtls_err );
      }
      if ( output_buffer != NULL )
      {
        free( output_buffer );
        output_buffer = NULL;
      }
      output_len = 0;
      break;
    case HTTP_EVENT_REDIRECT:
      LOG( PRINT_DEBUG, "HTTP_EVENT_REDIRECT" );
      esp_http_client_set_header( evt->client, "From", "user@example.com" );
      esp_http_client_set_header( evt->client, "Accept", "text/html" );
      esp_http_client_set_redirection( evt->client );
      break;

    default:
      break;
  }
  return ESP_OK;
}

static void _ota_event_handler( void* arg, esp_event_base_t event_base,
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
        ctx.downloaded_size = ( size_t ) * (int*) event_data;
        LOG( PRINT_DEBUG, "Writing to flash: %d written", *(int*) event_data );

        break;
      case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
        LOG( PRINT_INFO, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t*) event_data );
        break;
      case ESP_HTTPS_OTA_FINISH:
        LOG( PRINT_INFO, "OTA finish" );
        /*post result*/
        break;
      case ESP_HTTPS_OTA_ABORT:
        LOG( PRINT_INFO, "OTA abort" );
        /*post result*/
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

  // if ( memcmp( new_app_info->version, running_app_info.version, sizeof( new_app_info->version ) ) == 0 )
  // {
  //   LOG( PRINT_WARNING, "Current running version is the same as a new. We will not continue the update." );
  //   return ESP_FAIL;
  // }

  const uint32_t hw_sec_version = esp_efuse_read_secure_version();
  if ( new_app_info->secure_version < hw_sec_version )
  {
    LOG( PRINT_WARNING, "New firmware security version is less than eFuse programmed, %" PRIu32 " < %" PRIu32, new_app_info->secure_version, hw_sec_version );
    return ESP_FAIL;
  }

  return ESP_OK;
}

static esp_err_t _set_security_token( esp_http_client_handle_t http_client )
{
  char token[64] = {};
  char header[13 + sizeof( token )];
  OTAConfig_GetString( token, OTA_CONFIG_VALUE_TOKEN, sizeof( token ) );
  sprintf( header, "TargetToken %s", token );
  esp_err_t err = esp_http_client_set_header( http_client, "Authorization", header );
  return err;
}

static esp_err_t _http_client_init_cb( esp_http_client_handle_t http_client )
{
  return _set_security_token( http_client );
}

static void _set_update_error( void )
{
  LOG( PRINT_ERROR, "%s", ctx.update_result_details );
  ctx.ota_update_result = OTA_UPDATE_RESULT_FAILED;
  _send_internal_event( MSG_ID_OTA_POST_OTA_RESULT, NULL, 0 );
}

esp_http_client_handle_t _init_http_client( const char* url, esp_http_client_method_t method, const char* data, int len, const char* accept )
{
  memset( localResponseBuffer, 0, sizeof( localResponseBuffer ) );
  esp_http_client_config_t config = {
    .url = url,
    .event_handler = _http_event_handler,
    .user_data = localResponseBuffer,    // Pass address of local buffer to get response
    .disable_auto_redirect = true,
    .timeout_ms = 3000,
    .crt_bundle_attach = ota_bundle_attach,
  };

  esp_http_client_handle_t client = esp_http_client_init( &config );
  if ( client == NULL )
  {
    return NULL;
  }
  if ( ESP_OK != esp_http_client_set_method( client, method ) )
  {
    goto init_fail;
  }
  if ( accept != NULL )
  {
    if ( ESP_OK != esp_http_client_set_header( client, "Accept", accept ) )
    {
      goto init_fail;
    }
  }
  if ( ESP_OK != _set_security_token( client ) )
  {
    goto init_fail;
  }
  if ( data != NULL )
  {
    if ( ESP_OK != esp_http_client_set_header( client, "Content-Type", "application/json" ) || ESP_OK != esp_http_client_set_post_field( client, data, len ) )
    {
      goto init_fail;
    }
  }
  return client;
init_fail:
  esp_http_client_cleanup( client );
  return NULL;
}

static const char* _get_poll_address( void )
{
  OTAConfig_GetBool( &ctx.use_tls, OTA_CONFIG_VALUE_TLS );
  OTAConfig_GetString( ctx.address, OTA_CONFIG_VALUE_ADDRESS, sizeof( ctx.address ) );
  OTAConfig_GetString( ctx.tenant, OTA_CONFIG_VALUE_TENANT, sizeof( ctx.tenant ) );
  uint32_t sn = DevConfig_GetSerialNumber();
  snprintf( ctx.url, sizeof( ctx.url ), "%s%s/%s/controller/v1/%.6ld", ctx.use_tls ? "https://" : "http://",
            ctx.address, ctx.tenant, sn );
  return ctx.url;
}

static bool _post_ota_result( const char* result, const char* details )
{
  char post_data[512] = {};
  const char* base_url = _get_poll_address();
  const char* format = "{\"id\":\"%s\",\"status\":{\"result\":{\"finished\":\"%s\"},\"execution\":\"closed\",\"details\":[\"%s\"]}}";
  snprintf( urlDeploymentBaseFeedback, sizeof( urlDeploymentBaseFeedback ) - 1, "%s/deploymentBase/%s/feedback", base_url, ctx.action_id );
  snprintf( post_data, sizeof( post_data ) - 1, format, ctx.action_id, result, details );

  esp_http_client_handle_t client = _init_http_client( urlDeploymentBaseFeedback, HTTP_METHOD_POST, post_data, strlen( post_data ), NULL );
  if ( client == NULL )
  {
    return false;
  }
  esp_err_t err = esp_http_client_perform( client );
  if ( err == ESP_OK )
  {
    LOG( PRINT_INFO, "HTTP POST Status = %d, content_length = %" PRIu64,
         esp_http_client_get_status_code( client ),
         esp_http_client_get_content_length( client ) );
  }
  else
  {
    LOG( PRINT_ERROR, "HTTP POST request failed: %s", esp_err_to_name( err ) );
  }
  esp_http_client_cleanup( client );
  return err == ESP_OK;
}

#if 0
static bool _post_ota_progress( esp_http_client_handle_t client, uint32_t file_size, uint32_t downloaded )
{
  char post_data[512] = {};
  const char* format = "{\"id\":\"%s\",\"status\":{\"result\":{\"finished\":\"none\",\"progress\":{\"cnt\":%d,\"of\":%d}},\"execution\":\"proceeding\",\"details\":[\"%s\"]}}";

  snprintf( post_data, sizeof( post_data ) - 1, format, ctx.action_id, downloaded, file_size, "The update is being processed." );
  esp_http_client_set_post_field( client, post_data, strlen( post_data ) );

  esp_err_t err = esp_http_client_perform( client );
  if ( err == ESP_OK )
  {
    LOG( PRINT_INFO, "HTTP POST Status = %d, content_length = %" PRIu64,
         esp_http_client_get_status_code( client ),
         esp_http_client_get_content_length( client ) );
  }
  else
  {
    LOG( PRINT_ERROR, "HTTP POST request failed: %s", esp_err_to_name( err ) );
  }

  return err == ESP_OK;
}
#endif

static void _download_and_update_firmware( const char* url )
{
  esp_err_t ota_finish_err = ESP_OK;
  esp_http_client_config_t config = {
    .url = url,
    // .cert_pem = (char*) server_cert_pem_start,
    .timeout_ms = 3000,
    .keep_alive_enable = true,
    .crt_bundle_attach = ota_bundle_attach,
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
    sprintf( ctx.update_result_details, "ESP HTTPS OTA Begin failed" );
    _set_update_error();
    goto ota_end;
  }

  esp_app_desc_t app_desc;
  err = esp_https_ota_get_img_desc( https_ota_handle, &app_desc );
  if ( err != ESP_OK )
  {
    sprintf( ctx.update_result_details, "esp_https_ota_read_img_desc failed" );
    _set_update_error();
    goto ota_end;
  }
  err = _validate_image_header( &app_desc );
  if ( err != ESP_OK )
  {
    sprintf( ctx.update_result_details, "image header verification failed" );
    _set_update_error();
    goto ota_end;
  }

  ctx.file_size = esp_https_ota_get_image_size( https_ota_handle );
  LOG( PRINT_INFO, "Download image size %d", ctx.file_size );
  while ( 1 )
  {
    err = esp_https_ota_perform( https_ota_handle );
    if ( err != ESP_ERR_HTTPS_OTA_IN_PROGRESS )
    {
      break;
    }
    int download = esp_https_ota_get_image_len_read( https_ota_handle );
    LOG( PRINT_DEBUG, "Image bytes read: %d from %d", download, ctx.file_size );
  }

  if ( esp_https_ota_is_complete_data_received( https_ota_handle ) != true )
  {
    // the OTA image was not completely received and user can customize the response to this situation.
    LOG( PRINT_ERROR, "Complete data was not received." );
  }
  else
  {
    ota_finish_err = esp_https_ota_finish( https_ota_handle );
    if ( ( err == ESP_OK ) && ( ota_finish_err == ESP_OK ) )
    {
      LOG( PRINT_INFO, "ESP_HTTPS_OTA upgrade successful. Wait rebooting ..." );
      _change_state( DOWNLOADED );
      return;
    }
    else
    {
      if ( ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED )
      {
        sprintf( ctx.update_result_details, "Image validation failed, image is corrupted" );
      }
      else
      {
        sprintf( ctx.update_result_details, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err );
      }
      LOG( PRINT_ERROR, "%s", ctx.update_result_details );
      ctx.ota_update_result = OTA_UPDATE_RESULT_FAILED;
      _send_internal_event( MSG_ID_OTA_POST_OTA_RESULT, NULL, 0 );
    }
  }

ota_end:
  esp_https_ota_abort( https_ota_handle );
  LOG( PRINT_ERROR, "ESP_HTTPS_OTA upgrade failed" );
}

static void _ota_process( ota_artifacts_t* artifact )
{
  if ( strcmp( artifact->filename, "main.bin" ) == 0 )
  {
    _download_and_update_firmware( artifact->download_http );
  }
}

void _ota_apply_callback( void )
{
  _send_internal_event( MSG_ID_OTA_POLL_SERVER, NULL, 0 );
}

/* State machine functions -----------------------------------------------------*/

static void _state_disabled_init( const app_event_t* event )
{
  esp_event_handler_register( ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &_ota_event_handler, NULL );
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if ( esp_ota_get_state_partition( running, &ota_state ) == ESP_OK )
  {
    if ( ota_state == ESP_OTA_IMG_PENDING_VERIFY )
    {
      if ( esp_ota_mark_app_valid_cancel_rollback() == ESP_OK )
      {
        ctx.ota_update_result = OTA_UPDATE_RESULT_SUCCESS;
        LOG( PRINT_INFO, "App is valid, rollback cancelled successfully" );
      }
      else
      {
        LOG( PRINT_ERROR, "Failed to cancel rollback" );
      }
    }
  }
  _change_state( IDLE );
}

static void _state_idle_event_polling( const app_event_t* event )
{
  const char* url = _get_poll_address();

  esp_http_client_handle_t client = _init_http_client( url, HTTP_METHOD_GET, NULL, 0, "application/hal+json" );
  if ( client == NULL )
  {
    return;
  }

  esp_err_t err = esp_http_client_perform( client );
  AppTimerStart( timers, TIMER_ID_POLLING );

  if ( err == ESP_OK )
  {
    LOG( PRINT_INFO, "HTTP GET Status = %d, content_length = %" PRIu64,
         esp_http_client_get_status_code( client ),
         esp_http_client_get_content_length( client ) );
  }
  else
  {
    LOG( PRINT_ERROR, "HTTP GET request failed: %s", esp_err_to_name( err ) );
    esp_http_client_cleanup( client );
    return;
  }
  LOG( PRINT_INFO, "%s", localResponseBuffer );

  if ( false == OTAParser_ParseUrl( localResponseBuffer, urlConfigData, sizeof( urlConfigData ), urlDeploymentBase, sizeof( urlDeploymentBase ) ) )
  {
    esp_http_client_cleanup( client );
    return;
  }

  if ( strlen( urlConfigData ) != 0 )
  {
    _send_internal_event( MSG_ID_OTA_POST_CONFIG_DATA, NULL, 0 );
  }

  if ( strlen( urlDeploymentBase ) != 0 )
  {
    assert( _find_action_id( urlDeploymentBase, ctx.action_id, sizeof( ctx.action_id ) ) );

    if ( ctx.ota_update_result != OTA_UPDATE_RESULT_NONE )
    {
      _send_internal_event( MSG_ID_OTA_POST_OTA_RESULT, NULL, 0 );
    }
    else
    {
      _send_internal_event( MSG_ID_OTA_DOWNLOAD_IMAGE, NULL, 0 );
    }
  }

  esp_http_client_cleanup( client );
}

static void _state_idle_event_post_config_data( const app_event_t* event )
{
  LOG( PRINT_INFO, "post_config_data: %s", urlConfigData );
  const char* post_data = "{\"mode\":\"merge\",\"data\":{\"VIN\":\"JH4TB2H26CC000001\", \
                          \"hwRevision\":\"1\"},\"status\":{\"result\":{\"finished\":\"success\"}, \
                          \"execution\": \"closed\",\"details\":[]}}";
  esp_http_client_handle_t client = _init_http_client( urlConfigData, HTTP_METHOD_PUT, post_data, strlen( post_data ), "application/hal+json" );
  if ( client == NULL )
  {
    return;
  }

  esp_err_t err = esp_http_client_perform( client );
  if ( err == ESP_OK )
  {
    LOG( PRINT_INFO, "HTTP POST Status = %d, content_length = %" PRIu64,
         esp_http_client_get_status_code( client ),
         esp_http_client_get_content_length( client ) );
  }
  else
  {
    LOG( PRINT_ERROR, "HTTP POST request failed: %s", esp_err_to_name( err ) );
  }
  esp_http_client_cleanup( client );
}

static void _state_idle_event_ota_download_image( const app_event_t* event )
{
  LOG( PRINT_INFO, "_ota_process: %s", urlDeploymentBase );
  memset( &otaDeployment, 0, sizeof( otaDeployment ) );
  esp_http_client_handle_t client = _init_http_client( urlDeploymentBase, HTTP_METHOD_GET, NULL, 0, "application/hal+json" );
  if ( client == NULL )
  {
    return;
  }

  esp_err_t err = esp_http_client_perform( client );
  if ( err == ESP_OK )
  {
    LOG( PRINT_INFO, "HTTP GET Status = %d, content_length = %" PRIu64,
         esp_http_client_get_status_code( client ),
         esp_http_client_get_content_length( client ) );
    esp_http_client_cleanup( client );
  }
  else
  {
    LOG( PRINT_ERROR, "HTTP GET request failed: %s", esp_err_to_name( err ) );
    esp_http_client_cleanup( client );
    return;
  }

  LOG( PRINT_INFO, "%s", localResponseBuffer );

  OTAParse_ParseDeployment( localResponseBuffer, &otaDeployment );

  AppTimerStop( timers, TIMER_ID_POLLING );
  if ( otaDeployment.chunkSize != 0 )
  {
    for ( int chunk = 0; chunk < otaDeployment.chunkSize; chunk++ )
    {
      for ( int art = 0; art < otaDeployment.chunk[chunk].artifactsSize; art++ )
      {
        _ota_process( &otaDeployment.chunk[chunk].artifacts[art] );
      }
    }
  }
  AppTimerStart( timers, TIMER_ID_POLLING );
}

static void _state_idle_event_post_ota_result( const app_event_t* event )
{
  bool post_result = false;
  switch ( ctx.ota_update_result )
  {
    case OTA_UPDATE_RESULT_SUCCESS:
      post_result = _post_ota_result( "success", "The update was successfully installed." );
      break;

    case OTA_UPDATE_RESULT_FAILED:
      post_result = _post_ota_result( "failed", "The update was failed." );
      break;

    default:
      break;
  }

  if ( post_result )
  {
    ctx.ota_update_result = OTA_UPDATE_RESULT_NONE;
  }
}

static void _ota_task( void* pvParameter )
{
  LOG( PRINT_INFO, "Starting Advanced OTA example" );
  _send_internal_event( MSG_ID_INIT_REQ, NULL, 0 );
  while ( 1 )
  {
    app_event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      if ( false == AppEventSearchAndExecute( &event, module_state[ctx.state].event_handler_array, module_state[ctx.state].event_handler_array_size ) )
      {
        LOG( PRINT_INFO, "State: %s %d", _get_state_name( ctx.state ), module_state[ctx.state].event_handler_array_size );
      }
      AppEventDelete( &event );
    }
  }
}

void OTA_PostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void OTA_Init( void )
{
  OTAConfig_Init();
  OTAConfig_SetCallback( _ota_apply_callback );
  ctx.queue = xQueueCreate( 16, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( &_ota_task, "_ota_task", 1024 * 8, NULL, 5, NULL );
}
