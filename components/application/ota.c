/**
 *******************************************************************************
 * @file    wifidrv.c
 * @author  Dmytro Shevchenko
 * @brief   Wifi Drv
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "app_config.h"
#include "app_events.h"
#include "esp_efuse.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwjson.h"
#include "nvs.h"
#include "nvs_flash.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[OTA] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CONFIG_TCPIP_EVENT_THD_WA_SIZE 4096
#define OTA_URL_SIZE                   256
#define OTA_MAX_ARTIFACTS              3
#define OTA_MAX_CHUNKS                 3
#define SECURITY_TOKEN                 "d8542ad550fef419a6bf1189babf58ab"

/* Private types -------------------------------------------------------------*/
typedef enum
{
  ELinkTypeConfigData,
  ELinkTypeDeploymentBase,
  ELinkTypeLast
} TLinkType;

typedef enum
{
  OTA_DEPLOYMENT_ACTION_FORCED,
  OTA_DEPLOYMENT_ACTION_SOFT,
  OTA_DEPLOYMENT_ACTION_DOWNLOAD_ONLY,
  OTA_DEPLOYMENT_ACTION_TIME_FORCED,
  OTA_DEPLOYMENT_ACTION_UNKNOWN,
  OTA_DEPLOYMENT_ACTION_LAST
} ota_deployment_action_t;

typedef struct
{
  char filename[32];
  /* hashes not used */
  size_t size;
  char download_http[OTA_URL_SIZE];
  /* md5sum-http not used*/
} ota_artifacts_t;

typedef struct
{
  char part[32];
  char version[32];
  char name[32];
  size_t artifactsSize;
  ota_artifacts_t artifacts[OTA_MAX_CHUNKS];
} ota_chunk_t;

typedef struct
{
  ota_deployment_action_t download;
  ota_deployment_action_t update;
  size_t chunkSize;
  ota_chunk_t chunk[OTA_MAX_ARTIFACTS];
} ota_deployment_t;

/* Extern variables ----------------------------------------------------------*/
extern const uint8_t server_cert_pem_start[] asm( "_binary_ca_cert_pem_start" );
extern const uint8_t server_cert_pem_end[] asm( "_binary_ca_cert_pem_end" );

/* Private variables ---------------------------------------------------------*/
const char* ota_deployment_action_array[OTA_DEPLOYMENT_ACTION_LAST] =
  {
    [OTA_DEPLOYMENT_ACTION_FORCED] = "forced",
    [OTA_DEPLOYMENT_ACTION_SOFT] = "soft",
    [OTA_DEPLOYMENT_ACTION_DOWNLOAD_ONLY] = "downloadonly",
    [OTA_DEPLOYMENT_ACTION_TIME_FORCED] = "timeforced",
};

static char local_response_buffer[2048];
static char link_config_data[OTA_URL_SIZE];
static char link_deployment_base[OTA_URL_SIZE];
static uint32_t read_link_type;
static ota_deployment_t ota_deployment;

/* Private functions ---------------------------------------------------------*/
esp_err_t _http_event_handler( esp_http_client_event_t* evt )
{
  static char* output_buffer;    // Buffer to store response of http request from event handler
  static int output_len;    // Stores number of bytes read
  switch ( evt->event_id )
  {
    case HTTP_EVENT_ERROR:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ERROR" );
      break;
    case HTTP_EVENT_ON_CONNECTED:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_CONNECTED" );
      break;
    case HTTP_EVENT_HEADER_SENT:
      LOG( PRINT_DEBUG, "HTTP_EVENT_HEADER_SENT" );
      break;
    case HTTP_EVENT_ON_HEADER:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value );
      break;
    case HTTP_EVENT_ON_DATA:
      LOG( PRINT_DEBUG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len );
      /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
      if ( !esp_http_client_is_chunked_response( evt->client ) )
      {
        // If user_data buffer is configured, copy the response into the buffer
        int copy_len = 0;
        if ( evt->user_data )
        {
          copy_len = MIN( evt->data_len, ( sizeof( local_response_buffer ) - output_len ) );
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
        // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
        // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
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
  err = esp_http_client_set_header( http_client, "Authorization", "TargetToken " SECURITY_TOKEN );
  return err;
}

static bool _parse_string( lwjson_token_t* token, const char* strName, char* string, size_t stringSize )
{
  const char* readStr = NULL;
  size_t readStrSize = 0;
  if ( token->type == LWJSON_TYPE_STRING && 0 == strncmp( strName, token->token_name, token->token_name_len ) )
  {
    readStr = lwjson_get_val_string( token, &readStrSize );
    if ( readStr == NULL )
    {
      LOG( PRINT_ERROR, "Cannot found string %s", strName );
      return false;
    }

    if ( readStrSize > stringSize )
    {
      LOG( PRINT_ERROR, "Buffer to small for copy link" );
      return false;
    }

    memset( string, 0, stringSize );
    memcpy( string, readStr, readStrSize );
    return true;
  }
  return false;
}

static bool _get_link( lwjson_token_t* token, const char* linkName, char* link, size_t linkLen )
{
  LOG( PRINT_DEBUG, "Token child: %d %.*s", token->type, (int) token->token_name_len, token->token_name );
  if ( token->type == LWJSON_TYPE_OBJECT && 0 == strncmp( linkName, token->token_name, token->token_name_len ) )
  {
    lwjson_token_t* href = (lwjson_token_t*) lwjson_get_first_child( token );
    LOG( PRINT_DEBUG, "Token child: %.*s", (int) href->token_name_len, href->token_name );
    return _parse_string( href, "href", link, linkLen );
  }
  return false;
}

static bool _parse_link( const char* json_string )
{
  lwjson_token_t tokens[16];
  lwjson_t lwjson;
  lwjson_init( &lwjson, tokens, LWJSON_ARRAYSIZE( tokens ) );

  read_link_type = 0;

  if ( lwjson_parse_ex( &lwjson, json_string, strlen( json_string ) ) == lwjsonOK )
  {
    lwjson_token_t* t;

    /* Get very first token as top object */
    t = lwjson_get_first_token( &lwjson );
    if ( t->type != LWJSON_TYPE_OBJECT )
    {
      LOG( PRINT_ERROR, "Invalid json" );
      lwjson_free( &lwjson );
      return false;
    }

    for ( lwjson_token_t* tkn = (lwjson_token_t*) lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Type %d, Token: %.*s", tkn->type, (int) tkn->token_name_len, tkn->token_name );
      if ( tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "_links", tkn->token_name, tkn->token_name_len ) )
      {
        for ( lwjson_token_t* data = (lwjson_token_t*) lwjson_get_first_child( tkn ); data != NULL; data = data->next )
        {
          if ( ( ( read_link_type & ( 1 << ELinkTypeConfigData ) ) == 0 ) && _get_link( data, "configData", link_config_data, sizeof( link_config_data ) ) )
          {
            read_link_type |= ( 1 << ELinkTypeConfigData );
          }
          if ( ( ( read_link_type & ( 1 << ELinkTypeDeploymentBase ) ) == 0 ) && _get_link( data, "deploymentBase", link_deployment_base, sizeof( link_deployment_base ) ) )
          {
            read_link_type |= ( 1 << ELinkTypeDeploymentBase );
          }
        }
      }
    }
    lwjson_free( &lwjson );
  }

  return true;
}

static ota_deployment_action_t _get_action_type( const char* action )
{
  for ( int i = 0; i < OTA_DEPLOYMENT_ACTION_UNKNOWN; i++ )
  {
    if ( memcmp( ota_deployment_action_array[i], action, strlen( ota_deployment_action_array[i] ) ) == 0 )
    {
      return i;
    }
  }
  return OTA_DEPLOYMENT_ACTION_UNKNOWN;
}

static bool _parse_deployment( const char* json_string, ota_deployment_t* deployment )
{
  lwjson_token_t tokens[64];
  lwjson_t lwjson;
  lwjson_init( &lwjson, tokens, LWJSON_ARRAYSIZE( tokens ) );

  read_link_type = 0;

  if ( lwjson_parse_ex( &lwjson, json_string, strlen( json_string ) ) == lwjsonOK )
  {
    lwjson_token_t* t;

    /* Get very first token as top object */
    t = lwjson_get_first_token( &lwjson );
    if ( t->type != LWJSON_TYPE_OBJECT )
    {
      LOG( PRINT_ERROR, "Invalid json" );
      lwjson_free( &lwjson );
      return false;
    }
    /* Main object */
    for ( lwjson_token_t* tkn = (lwjson_token_t*) lwjson_get_first_child( t ); tkn != NULL; tkn = tkn->next )
    {
      LOG( PRINT_DEBUG, "Type %d, Token: %.*s", tkn->type, (int) tkn->token_name_len, tkn->token_name );
      if ( tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "deployment", tkn->token_name, tkn->token_name_len ) )
      {
        /* Deployment */
        for ( lwjson_token_t* child_token = (lwjson_token_t*) lwjson_get_first_child( tkn ); child_token != NULL; child_token = child_token->next )
        {
          if ( child_token->type == LWJSON_TYPE_STRING )
          {
            char string[32] = {};
            if ( _parse_string( child_token, "update", string, sizeof( string ) ) )
            {
              LOG( PRINT_DEBUG, "update: %s", string );
              deployment->update = _get_action_type( string );
              continue;
            }
            else if ( _parse_string( child_token, "download", string, sizeof( string ) ) )
            {
              LOG( PRINT_DEBUG, "download: %s", string );
              deployment->download = _get_action_type( string );
              continue;
            }
          }
          else if ( child_token->type == LWJSON_TYPE_ARRAY && 0 == strncmp( "chunks", child_token->token_name, child_token->token_name_len ) )
          {
            uint8_t index = 0;
            for ( lwjson_token_t* chunk_tkn = (lwjson_token_t*) lwjson_get_first_child( child_token ); chunk_tkn != NULL; chunk_tkn = chunk_tkn->next )
            {
              for ( lwjson_token_t* chunk_obj_tkn = (lwjson_token_t*) lwjson_get_first_child( chunk_tkn ); chunk_obj_tkn != NULL; chunk_obj_tkn = chunk_obj_tkn->next )
              {
                if ( _parse_string( chunk_obj_tkn, "part", deployment->chunk[index].part, sizeof( deployment->chunk[index].part ) ) )
                {
                  LOG( PRINT_DEBUG, "part: %s", deployment->chunk[index].part );
                }
                else if ( _parse_string( chunk_obj_tkn, "version", deployment->chunk[index].version, sizeof( deployment->chunk[index].version ) ) )
                {
                  LOG( PRINT_DEBUG, "version: %s", deployment->chunk[index].version );
                }
                else if ( _parse_string( chunk_obj_tkn, "name", deployment->chunk[index].name, sizeof( deployment->chunk[index].name ) ) )
                {
                  LOG( PRINT_DEBUG, "name: %s", deployment->chunk[index].name );
                }
                else if ( chunk_obj_tkn->type == LWJSON_TYPE_ARRAY && 0 == strncmp( "artifacts", chunk_obj_tkn->token_name, chunk_obj_tkn->token_name_len ) )
                {
                  uint8_t art_index = 0;
                  for ( lwjson_token_t* art_tkn = (lwjson_token_t*) lwjson_get_first_child( chunk_obj_tkn ); art_tkn != NULL; art_tkn = art_tkn->next )
                  {
                    for ( lwjson_token_t* art_obj_tkn = (lwjson_token_t*) lwjson_get_first_child( art_tkn ); art_obj_tkn != NULL; art_obj_tkn = art_obj_tkn->next )
                    {
                      if ( _parse_string( art_obj_tkn, "filename", deployment->chunk[index].artifacts[art_index].filename, sizeof( deployment->chunk[index].artifacts[art_index].filename ) ) )
                      {
                        LOG( PRINT_DEBUG, "filename: %s", deployment->chunk[index].artifacts[art_index].filename );
                      }
                      else if ( art_obj_tkn->type == LWJSON_TYPE_NUM_INT && 0 == strncmp( "size", art_obj_tkn->token_name, art_obj_tkn->token_name_len ) )
                      {
                        deployment->chunk[index].artifacts[art_index].size = lwjson_get_val_int( art_obj_tkn );
                        LOG( PRINT_DEBUG, "size: %d", deployment->chunk[index].artifacts[art_index].size );
                      }
                      else if ( art_obj_tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "_links", art_obj_tkn->token_name, art_obj_tkn->token_name_len ) )
                      {
                        for ( lwjson_token_t* link = (lwjson_token_t*) lwjson_get_first_child( art_obj_tkn ); link != NULL; link = link->next )
                        {
                          _get_link( link, "download-http", deployment->chunk[index].artifacts[art_index].download_http, sizeof( deployment->chunk[index].artifacts[art_index].download_http ) );
                        }
                      }
                    }
                    art_index++;
                  }
                  deployment->chunk[index].artifactsSize = art_index;
                }
              }
              index++;
            }
            deployment->chunkSize = index;
          }
        }
      }
    }
    lwjson_free( &lwjson );
  }

  return true;
}

static bool _post_config_data( esp_http_client_handle_t client )
{
  LOG( PRINT_INFO, "Link: %s", link_config_data );
  // POST
  const char* post_data = "{\"mode\":\"merge\",\"data\":{\"VIN\":\"JH4TB2H26CC000001\", \
                          \"hwRevision\":\"1\"},\"status\":{\"result\":{\"finished\":\"success\"}, \
                          \"execution\": \"closed\",\"details\":[]}}";
  esp_http_client_set_url( client, link_config_data );
  esp_http_client_set_method( client, HTTP_METHOD_PUT );
  esp_http_client_set_header( client, "Content-Type", "application/json" );
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
    return false;
  }
  return true;
}

static bool _register_device( void )
{
  esp_http_client_config_t config = {
    .url = "http://192.168.1.136:8080/default/controller/v1/AAD_000001",
    .event_handler = _http_event_handler,
    .user_data = local_response_buffer,    // Pass address of local buffer to get response
    .disable_auto_redirect = true,
    .timeout_ms = 3000,
  };

  esp_http_client_handle_t client = esp_http_client_init( &config );
  esp_http_client_set_header( client, "Authorization", "TargetToken " SECURITY_TOKEN );
  esp_http_client_set_header( client, "Accept", "application/hal+json" );
  esp_err_t err = esp_http_client_perform( client );
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
    return false;
  }
  LOG( PRINT_INFO, "%s", local_response_buffer );

  if ( false == _parse_link( local_response_buffer ) )
  {
    esp_http_client_cleanup( client );
    return false;
  }

  if ( ( read_link_type & ( 1 << ELinkTypeConfigData ) ) != 0 )
  {
    _post_config_data( client );
  }

  if ( ( read_link_type & ( 1 << ELinkTypeDeploymentBase ) ) != 0 )
  {
    LOG( PRINT_INFO, "Link: %s", link_deployment_base );
    esp_http_client_set_url( client, link_deployment_base );
    esp_http_client_set_method( client, HTTP_METHOD_GET );
    esp_http_client_set_post_field( client, NULL, 0 );
    esp_err_t err = esp_http_client_perform( client );
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
      return false;
    }

    LOG( PRINT_INFO, "%s", local_response_buffer );

    _parse_deployment( local_response_buffer, &ota_deployment );
  }

  esp_http_client_cleanup( client );
  return true;
}

static void update_firmware( const char* url )
{
  esp_err_t ota_finish_err = ESP_OK;
  esp_http_client_config_t config = {
    .url = url,
    // .cert_pem = (char*) server_cert_pem_start,
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
}

static void ota_process( ota_artifacts_t* artifact )
{
  if ( strcmp( artifact->filename, "main.bin" ) == 0 )
  {
    update_firmware( artifact->download_http );
  }
}

static void _ota_task( void* pvParameter )
{
  LOG( PRINT_INFO, "Starting Advanced OTA example" );
  memset( &ota_deployment, 0, sizeof( ota_deployment ) );
  _register_device();
  if ( ota_deployment.chunkSize != 0 )
  {
    for ( int chunk = 0; chunk < ota_deployment.chunkSize; chunk++ )
    {
      for ( int art = 0; art < ota_deployment.chunk[chunk].artifactsSize; art++ )
      {
        ota_process(&ota_deployment.chunk[chunk].artifacts[art]);
      }
    }
  }
  vTaskDelete( NULL );
}

void OTA_Init( void )
{
  esp_event_handler_register( ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &_ota_event_handler, NULL );

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
