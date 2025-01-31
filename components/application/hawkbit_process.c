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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hawkbit_config.h"
#include "hawkbit_parser.h"
#include "mongoose_task.h"
#include "ota_drv.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[HAWKBIT_P] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_HTTP_HAWKBIT
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
  HAWKBIT_POLL_SERVER,
  HAWKBIT_POST_CONFIG_DATA,
  HAWKBIT_DOWNLOAD_IMAGE,
  HAWKBIT_POST_HAWKBIT_RESULT,
  HAWKBIT_STOP_ACTION_ID,
} event_t;

typedef enum
{
  HAWKBIT_UPDATE_RESULT_NONE,
  HAWKBIT_UPDATE_RESULT_SUCCESS,
  HAWKBIT_UPDATE_RESULT_FAILED
} hawkbit_update_result_t;

typedef struct
{
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

/* Private variables ---------------------------------------------------------*/
static char localResponseBuffer[2048];
static int responseCode;
static char urlConfigData[HAWKBIT_URL_SIZE];
static char urlDeploymentBase[HAWKBIT_URL_SIZE];
static char urlDeploymentBaseFeedback[HAWKBIT_URL_SIZE + 16];
static char urlCancelAction[HAWKBIT_URL_SIZE];
static hawkbit_deployment_t hawkbitDeployment;
static TimerHandle_t polling_timer;
static TaskHandle_t hawkbit_task_handle = NULL;

static module_ctx_t ctx;
static struct mg_connection* client_conn = NULL;
static SemaphoreHandle_t http_semaphore;

/* Private functions ---------------------------------------------------------*/
static void _send_internal_event( event_t id )
{
  if ( xQueueSend( ctx.queue, (void*) &id, 0 ) != pdPASS )
  {
    assert( 0 );
  }
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
  _send_internal_event( HAWKBIT_POLL_SERVER );
}

static void _http_event_handler( struct mg_connection* c, int ev, void* ev_data )
{
  struct mg_http_message* hm = (struct mg_http_message*) ev_data;
  switch ( ev )
  {
    case MG_EV_HTTP_MSG:
      responseCode = mg_http_status( hm );
      LOG( PRINT_DEBUG, "HTTP response: %d %.*s", responseCode, (int) hm->body.len, hm->body.buf );
      if ( responseCode == 200 )
      {
        strncpy( localResponseBuffer, hm->body.buf, MIN( sizeof( localResponseBuffer ) - 1, hm->body.len ) );
        localResponseBuffer[MIN( sizeof( localResponseBuffer ) - 1, hm->body.len )] = '\0';
      }
      c->is_closing = 1;
      xSemaphoreGive( http_semaphore );
      break;
    case MG_EV_ERROR:
      LOG( PRINT_ERROR, "HTTP request failed" );
      c->is_closing = 1;
      responseCode = -1;
      xSemaphoreGive( http_semaphore );
      break;
    default:
      break;
  }
}

static void _http_perform( const char* url, const char* method, const char* data, const char* accept )
{
  client_conn = mg_http_connect( &mgr, url, _http_event_handler, NULL );
  if ( client_conn == NULL )
  {
    LOG( PRINT_ERROR, "Failed to initialize HTTP client" );
    return;
  }

  char token[64] = {};
  struct mg_str host = mg_url_host( url );
  int port = mg_url_port( url );
  HAWKBITConfig_GetString( token, HAWKBIT_CONFIG_VALUE_TOKEN, sizeof( token ) );
  static char test[2048] = {};

  if ( strcmp( method, "GET" ) == 0 )
  {
    sprintf( test, "GET %s HTTP/1.1\r\nHost: %.*s:%d\r\nAuthorization: GatewayToken %s\r\nAccept: %s\r\n\r\n", url, host.len, host.buf, port, token, accept ? accept : "application/json" );
    LOG( PRINT_DEBUG, "HTTP request: %s", test );
    mg_printf( client_conn, "%s", test );
  }
  else if ( strcmp( method, "POST" ) == 0 )
  {
    sprintf( test, "POST %s HTTP/1.1\r\nHost: %.*s:%d\r\nAuthorization: GatewayToken %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", url, host.len, host.buf, port, token, strlen( data ) );
    LOG( PRINT_DEBUG, "HTTP request:\n%s\ndata: %s", test, data );
    mg_printf( client_conn, "%s%s", test, data );
  }
  else if ( strcmp( method, "PUT" ) == 0 )
  {
    sprintf( test, "PUT %s HTTP/1.1\r\nHost: %.*s:%d\r\nAuthorization: GatewayToken %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n", url, host.len, host.buf, port, token, strlen( data ) );
    LOG( PRINT_DEBUG, "HTTP request:\n%s\ndata: %s", test, data );
    mg_printf( client_conn, "%s%s", test, data );
  }
}

static void _http_perform_and_wait( const char* url, const char* method, const char* data, const char* accept )
{
  _http_perform( url, method, data, accept );

  // Wait for the response
  xSemaphoreTake( http_semaphore, portMAX_DELAY );
}

static const char* _get_poll_address( void )
{
  HAWKBITConfig_GetBool( &ctx.use_tls, HAWKBIT_CONFIG_VALUE_TLS );
  HAWKBITConfig_GetString( ctx.address, HAWKBIT_CONFIG_VALUE_ADDRESS, sizeof( ctx.address ) );
  HAWKBITConfig_GetString( ctx.tenant, HAWKBIT_CONFIG_VALUE_TENANT, sizeof( ctx.tenant ) );
  const char* sn = DevConfig_GetSerialNumber();
  snprintf( ctx.url, sizeof( ctx.url ), "%s/%s/controller/v1/%s",
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

  _http_perform_and_wait( urlDeploymentBaseFeedback, "POST", post_data, NULL );

  if ( responseCode != 200 )
  {
    LOG( PRINT_ERROR, "HTTP POST request failed" );
    return false;
  }

  LOG( PRINT_INFO, "HTTP POST Status = 200, content_length = %d", strlen( localResponseBuffer ) );
  return true;
}

static void _download_and_update_firmware( const char* url )
{
  bool result = OTA_Download( url );
  ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_FAILED;
  if ( false == result )
  {
    LOG( PRINT_ERROR, "OTA failed to starting download" );
    _send_internal_event( HAWKBIT_POST_HAWKBIT_RESULT );
    return;
  }

  while ( OTA_GetState() == OTA_DRIVER_STATE_DOWNLOAD )
  {
    osDelay( 50 );
  }

  if ( OTA_GetState() == OTA_DRIVER_STATE_DOWNLOAD_FINISHED )
  {
    ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_SUCCESS;
    return;
  }

  _send_internal_event( HAWKBIT_POST_HAWKBIT_RESULT );
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
  _send_internal_event( HAWKBIT_POLL_SERVER );
}

/* State machine functions -----------------------------------------------------*/

static void _init( void )
{
  ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_SUCCESS;
  _send_internal_event( HAWKBIT_POLL_SERVER );
}

static void _handle_cancel_action( void )
{
  _http_perform_and_wait( urlCancelAction, "GET", NULL, "application/hal+json" );

  if ( responseCode != 200 )
  {
    LOG( PRINT_ERROR, "HTTP GET request failed" );
    return;
  }

  LOG( PRINT_INFO, "%s", localResponseBuffer );

  int actionId;
  if ( HAWKBITParser_ParseCancelAction( localResponseBuffer, &actionId ) )
  {
    char actionIdStr[32];
    snprintf( actionIdStr, sizeof( actionIdStr ), "%d", actionId );
    if ( strcmp( ctx.action_id, actionIdStr ) == 0 )
    {
      LOG( PRINT_INFO, "Canceled Action ID: %s", ctx.action_id );
      ctx.hawkbit_update_result = HAWKBIT_UPDATE_RESULT_FAILED;
      _send_internal_event( HAWKBIT_POST_HAWKBIT_RESULT );
    }
  }
  else
  {
    LOG( PRINT_ERROR, "Failed to parse cancel action response" );
  }
}

static void _polling( void )
{
  const char* url = _get_poll_address();
  _http_perform_and_wait( url, "GET", NULL, "application/hal+json" );
  xTimerStart( polling_timer, 0 );

  if ( responseCode != 200 )
  {
    LOG( PRINT_ERROR, "HTTP GET request failed" );
    return;
  }

  LOG( PRINT_INFO, "%s", localResponseBuffer );

  if ( !HAWKBITParser_ParseUrl( localResponseBuffer, urlConfigData, sizeof( urlConfigData ), urlDeploymentBase, sizeof( urlDeploymentBase ), urlCancelAction, sizeof( urlCancelAction ) ) )
  {
    return;
  }

  if ( strlen( urlConfigData ) != 0 )
  {
    _send_internal_event( HAWKBIT_POST_CONFIG_DATA );
  }

  if ( strlen( urlDeploymentBase ) != 0 )
  {
    assert( _find_action_id( urlDeploymentBase, ctx.action_id, sizeof( ctx.action_id ) ) );

    if ( ctx.hawkbit_update_result != HAWKBIT_UPDATE_RESULT_NONE )
    {
      _send_internal_event( HAWKBIT_POST_HAWKBIT_RESULT );
    }
    else
    {
      _send_internal_event( HAWKBIT_DOWNLOAD_IMAGE );
    }
  }

  if ( strlen( urlCancelAction ) != 0 )
  {
    LOG( PRINT_INFO, "Cancel action URL found: %s", urlCancelAction );
    _send_internal_event( HAWKBIT_STOP_ACTION_ID );
  }
}

static void _post_config_data( void )
{
  LOG( PRINT_INFO, "post_config_data: %s", urlConfigData );
  const char* post_data = "{\"mode\":\"merge\",\"data\":{\"VIN\":\"JH4TB2H26CC000001\","
                          "\"hwRevision\":\"1\"},\"status\":{\"result\":{\"finished\":\"success\"},"
                          "\"execution\": \"closed\",\"details\":[]}}";
  _http_perform_and_wait( urlConfigData, "PUT", post_data, "application/hal+json" );

  if ( responseCode != 200 )
  {
    LOG( PRINT_ERROR, "HTTP PUT request failed" );
  }
  else
  {
    LOG( PRINT_INFO, "HTTP PUT Status = 200, content_length = %d", strlen( localResponseBuffer ) );
  }
}

static void _hawkbit_download_image( void )
{
  LOG( PRINT_INFO, "_hawkbit_process: %s", urlDeploymentBase );
  memset( &hawkbitDeployment, 0, sizeof( hawkbitDeployment ) );
  _http_perform_and_wait( urlDeploymentBase, "GET", NULL, "application/hal+json" );

  if ( responseCode != 200 )
  {
    LOG( PRINT_ERROR, "HTTP GET request failed" );
    return;
  }

  LOG( PRINT_INFO, "%s", localResponseBuffer );

  HAWKBITParse_ParseDeployment( localResponseBuffer, &hawkbitDeployment );

  xTimerStop( polling_timer, 0 );
  if ( hawkbitDeployment.chunkSize != 0 )
  {
    for ( int chunk = 0; chunk < hawkbitDeployment.chunkSize; chunk++ )
    {
      for ( int art = 0; chunk < hawkbitDeployment.chunk[chunk].artifactsSize; art++ )
      {
        _hawkbit_process( &hawkbitDeployment.chunk[chunk].artifacts[art] );
      }
    }
  }
  xTimerStart( polling_timer, 0 );
}

static void _event_post_hawkbit_result( void )
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
        case HAWKBIT_POLL_SERVER:
          _polling();
          break;
        case HAWKBIT_POST_CONFIG_DATA:
          _post_config_data();
          break;
        case HAWKBIT_DOWNLOAD_IMAGE:
          _hawkbit_download_image();
          break;
        case HAWKBIT_POST_HAWKBIT_RESULT:
          _event_post_hawkbit_result();
          break;
        case HAWKBIT_STOP_ACTION_ID:
          _handle_cancel_action();
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

  polling_timer = xTimerCreate( "PollingTimer", pdMS_TO_TICKS( 300000 ), pdTRUE, NULL, _timer_polling );
  assert( polling_timer );

  http_semaphore = xSemaphoreCreateBinary();
  assert( http_semaphore );

  xTaskCreate( &_task, "_hawkbit_task", 1024 * 6, NULL, 5, &hawkbit_task_handle );
}

void HawkbitProcess_Deinit( void )
{
  if ( polling_timer != NULL )
  {
    xTimerStop( polling_timer, 0 );
    xTimerDelete( polling_timer, 0 );
    polling_timer = NULL;
  }

  if ( ctx.queue != NULL )
  {
    vQueueDelete( ctx.queue );
    ctx.queue = NULL;
  }

  if ( hawkbit_task_handle != NULL )
  {
    vTaskDelete( hawkbit_task_handle );
    hawkbit_task_handle = NULL;
  }

  if ( client_conn != NULL )
  {
    client_conn->is_closing = 1;
    client_conn = NULL;
  }

  if ( http_semaphore != NULL )
  {
    vSemaphoreDelete( http_semaphore );
    http_semaphore = NULL;
  }

  memset( &ctx, 0, sizeof( ctx ) );
}
