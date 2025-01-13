/**
 *******************************************************************************
 * @file    hawkbit.c
 * @author  Dmytro Shevchenko
 * @brief   OTA source file
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "hawkbit_process.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "app_config.h"
#include "dev_config.h"
#include "esp_crt_bundle.h"
#include "esp_efuse.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hawkbit_config.h"
#include "hawkbit_parser.h"
#include "ota_drv.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[HAWKBIT_P] "
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

/* Public types ----------------------------------------------------------*/

typedef enum
{
  MSG_ID_HAWKBIT_POLL_SERVER,
  MSG_ID_HAWKBIT_POST_CONFIG_DATA,
  MSG_ID_HAWKBIT_DOWNLOAD_IMAGE,
  MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT,
  MSG_ID_HAWKBIT_STOP_POLL_SERVER,
} event_t;

typedef enum
{
  HAWKBIT_UPDATE_RESULT_NONE,
  HAWKBIT_UPDATE_RESULT_SUCCESS,
  HAWKBIT_UPDATE_RESULT_FAILED
} hawkbit_update_result_t;

typedef struct
{
  module_state_t state;
  QueueHandle_t queue;
  hawkbit_update_result_t hawkbit_update_result;
  char update_result_details[256];
  char action_id[32];

  int file_size;
  int downloaded_size;
  uint8_t download_percent;

  char url[256];
  bool use_tls;
  char address[HAWKBIT_CONFIG_STR_SIZE];
  char tenant[HAWKBIT_CONFIG_STR_SIZE];

} module_ctx_t;

typedef enum
{
  TIMER_ID_POLLING,
  TIMER_ID_LAST
} timer_id;

/* Private functions declaration ---------------------------------------------*/
static void _timer_polling( TimerHandle_t xTimer );

static void _init( const event_t* event );
static void _polling( const event_t* event );
static void _post_config_data( const event_t* event );
static void _hawkbit_download_image( const event_t* event );
static void _post_hawkbit_result( const event_t* event );

static const struct app_events_handler _downloaded_state_handler_array[] =
  {
    case MSG_ID_DEINIT_REQ, _polling ),
};

/* Private variables ---------------------------------------------------------*/
static char localResponseBuffer[2048];
static char urlConfigData[HAWKBIT_URL_SIZE];
static char urlDeploymentBase[HAWKBIT_URL_SIZE];
static char urlDeploymentBaseFeedback[HAWKBIT_URL_SIZE + 16];
static hawkbit_deployment_t hawkbitDeployment;

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

static void _send_internal_event( event_t id )
{
  if ( xQueueSend( ctx.queue, (void*) &id, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

static esp_err_t _bundle_attach( void* conf )
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
  return result;
}

static void _timer_polling( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_HAWKBIT_POLL_SERVER );
}

static esp_err_t _http_event_handler( esp_http_client_event_t* evt )
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

static esp_err_t _set_security_token( esp_http_client_handle_t http_client )
{
  char token[64] = {};
  char header[13 + sizeof( token )];
  HAWKBITConfig_GetString( token, HAWKBIT_CONFIG_VALUE_TOKEN, sizeof( token ) );
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
  ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_FAILED;
  _send_internal_event( MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT );
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
    .crt_bundle_attach = _bundle_attach,
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
  HAWKBITConfig_GetBool( &ctx.use_tls, HAWKBIT_CONFIG_VALUE_TLS );
  HAWKBITConfig_GetString( ctx.address, HAWKBIT_CONFIG_VALUE_ADDRESS, sizeof( ctx.address ) );
  HAWKBITConfig_GetString( ctx.tenant, HAWKBIT_CONFIG_VALUE_TENANT, sizeof( ctx.tenant ) );
  uint32_t sn = DevConfig_GetSerialNumber();
  snprintf( ctx.url, sizeof( ctx.url ), "%s%s/%s/controller/v1/%.6ld", ctx.use_tls ? "https://" : "http://",
            ctx.address, ctx.tenant, sn );
  return ctx.url;
}

static bool _post_hawkbit_result( const char* result, const char* details )
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

static void _download_and_update_firmware( const char* url )
{
  esp_err_t hawkbit_finish_err = ESP_OK;
  bool result = OTA_Download( url );
  ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_FAILED;
  if ( false == result )
  {
    LOG( PRINT_ERROR, "OTA failed to starting download" );
    _send_internal_event( MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT );
    return;
  }

  while ( OTA_GetState() == OTA_DRIVER_STATE_DOWNLOAD )
  {
    osDelay( 50 );
  }

  if ( OTA_GetState() == OTA_DRIVER_STATE_DOWNLOAD_FINISHED )
  {
    ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_SUCCESS;
    _change_state( DOWNLOADED );
    return;
  }

  _send_internal_event( MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT );
}

static void _hawkbit_process( hawkbit_artifacts_t* artifact )
{
  if ( strcmp( artifact->filename, "main.bin" ) == 0 )
  {
    _download_and_update_firmware( artifact->download_http );
  }
}

void _hawkbit_apply_callback( void )
{
  _send_internal_event( MSG_ID_HAWKBIT_POLL_SERVER );
}

/* State machine functions -----------------------------------------------------*/

static void _init( const event_t* event )
{
  ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_SUCCESS;
  _change_state( IDLE );
}

static void _polling( const event_t* event )
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

  if ( false == HAWKBITParser_ParseUrl( localResponseBuffer, urlConfigData, sizeof( urlConfigData ), urlDeploymentBase, sizeof( urlDeploymentBase ) ) )
  {
    esp_http_client_cleanup( client );
    return;
  }

  if ( strlen( urlConfigData ) != 0 )
  {
    _send_internal_event( MSG_ID_HAWKBIT_POST_CONFIG_DATA );
  }

  if ( strlen( urlDeploymentBase ) != 0 )
  {
    assert( _find_action_id( urlDeploymentBase, ctx.action_id, sizeof( ctx.action_id ) ) );

    if ( ctx.hawkbit_update_result != HAWKBIT_UPDATE_RESULT_NONE )
    {
      _send_internal_event( MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT );
    }
    else
    {
      _send_internal_event( MSG_ID_HAWKBIT_DOWNLOAD_IMAGE );
    }
  }

  esp_http_client_cleanup( client );
}

static void _post_config_data( const event_t* event )
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

static void _hawkbit_download_image( const event_t* event )
{
  LOG( PRINT_INFO, "_hawkbit_process: %s", urlDeploymentBase );
  memset( &hawkbitDeployment, 0, sizeof( hawkbitDeployment ) );
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

  HAWKBITParse_ParseDeployment( localResponseBuffer, &hawkbitDeployment );

  AppTimerStop( timers, TIMER_ID_POLLING );
  if ( hawkbitDeployment.chunkSize != 0 )
  {
    for ( int chunk = 0; chunk < hawkbitDeployment.chunkSize; chunk++ )
    {
      for ( int art = 0; art < hawkbitDeployment.chunk[chunk].artifactsSize; art++ )
      {
        _hawkbit_process( &hawkbitDeployment.chunk[chunk].artifacts[art] );
      }
    }
  }
  AppTimerStart( timers, TIMER_ID_POLLING );
}

static void _post_hawkbit_result( const event_t* event )
{
  bool post_result = false;
  switch ( ctx.hawkbit_update_result )
  {
    case HAWKBIT_UPDATE_RESULT_SUCCESS:
      post_result = _post_hawkbit_result( "success", "The update was successfully installed." );
      break;

    case HAWKBIT_UPDATE_RESULT_FAILED:
      post_result = _post_hawkbit_result( "failed", "The update was failed." );
      break;

    default:
      break;
  }

  if ( post_result )
  {
    ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_NONE;
  }
}

static void _task( void* pvParameter )
{
  LOG( PRINT_INFO, "Starting HAWKBIT task" );
  _init();
  while ( 1 )
  {
    event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      switch ( event )
      {
        case MSG_ID_HAWKBIT_POLL_SERVER:
          _polling();
          break;
        case MSG_ID_HAWKBIT_POST_CONFIG_DATA:
          _post_config_data() break;
        case MSG_ID_HAWKBIT_DOWNLOAD_IMAGE:
          _hawkbit_download_image();
          break;
        case MSG_ID_HAWKBIT_POST_HAWKBIT_RESULT:
          _post_hawkbit_result();
          break;
        default:
          assert( 0 );
      }
    }
  }
}

void HawkbitProcess_Init( void )
{
  HAWKBITConfig_Init();
  HAWKBITConfig_SetCallback( _hawkbit_apply_callback );
  OTA_Init();
  ctx.queue = xQueueCreate( 8, sizeof( event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( &_task, "_hawkbit_task", 1024 * 6, NULL, 5, NULL );
}
