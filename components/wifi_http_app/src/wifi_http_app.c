/**
 *******************************************************************************
 * @file    wifi_http_app.c
 * @author  Dmytro Shevchenko
 * @brief   Wifi http application
 *******************************************************************************
 */

#include "wifi_http_app.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mongoose.h"
#include "wifidrv.h"

#define MODULE_NAME "[WifiHttp] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_NETWORK_MANAGER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#ifndef HTTP_WIFI_MANAGER_URL
#define HTTP_WIFI_MANAGER_URL "http://0.0.0.0:80"
#endif

#define WEBAPP_LOCATION "/"

/* const httpd related values stored in ROM */
#define http_404_hdr                "404 Not Found\r\n"
#define http_503_hdr                "503 Service Unavailable\r\n"
#define http_content_type_html      "Content-Type: text/html\r\n"
#define http_content_type_js        "Content-Type: text/javascript\r\n"
#define http_content_type_css       "Content-Type: text/css\r\n"
#define http_content_type_json      "Content-Type: application/json\r\n"
#define http_cache_control_hdr      "Cache-Control: "
#define http_cache_control_no_cache "no-store, no-cache, must-revalidate, max-age=0\r\n"
#define http_cache_control_cache    "public, max-age=31536000\r\n"
#define http_pragma_hdr             "Pragma: "
#define http_pragma_no_cache        "no-cache\r\n"

#define DEFAULT_HEADERS http_content_type_json       \
  http_cache_control_hdr http_cache_control_no_cache \
    http_pragma_hdr http_pragma_no_cache

/* strings holding the URLs of the wifi manager */
static char* http_root_url = NULL;
static char* http_redirect_url = NULL;
static char* http_js_url = NULL;
static char* http_css_url = NULL;
static char* http_connect_url = NULL;
static char* http_ap_url = NULL;
static char* http_status_url = NULL;

/* Variable for end and delete task */
static bool http_app_is_started;
static bool http_app_is_task_run;

/**
 * @brief embedded binary data.
 * @see file "component.mk"
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
extern const char style_css_start[] asm( "_binary_style_css_start" );
extern const char style_css_end[] asm( "_binary_style_css_end" );
extern const char code_js_start[] asm( "_binary_code_js_start" );
extern const char code_js_end[] asm( "_binary_code_js_end" );
extern const char index_html_start[] asm( "_binary_index_html_start" );
extern const char index_html_end[] asm( "_binary_index_html_end" );

static void http_server_delete_handler( struct mg_connection* c, struct mg_str* uri, struct mg_http_message* data )
{
  LOG( PRINT_DEBUG, "DELETE %.*s", uri->len, uri->buf );

  /* DELETE /connect.json */
  if ( mg_strcasecmp( *uri, mg_str( http_connect_url ) ) == 0 )
  {
    wifiDrvDisconnect();
    mg_http_reply( c, 200, DEFAULT_HEADERS, "" );
  }
  else
  {
    mg_http_reply( c, 404, "", http_404_hdr );
  }
}

static void http_server_post_handler( struct mg_connection* c, struct mg_str* uri, struct mg_http_message* data )
{
  LOG( PRINT_DEBUG, "POST %.*s", uri->len, uri->buf );

  /* POST /connect.json */
  if ( mg_strcasecmp( *uri, mg_str( http_connect_url ) ) == 0 )
  {
    /* len of values provided */
    struct mg_str* mg_ssid = mg_http_get_header( data, "X-Custom-ssid" );
    struct mg_str* mg_password = mg_http_get_header( data, "X-Custom-pwd" );

    if ( mg_ssid != NULL && mg_password != NULL )
    {
      wifiDrvSetAPName( mg_ssid->buf, mg_ssid->len );
      wifiDrvSetPassword( mg_password->buf, mg_password->len );
      wifiDrvConnect();

      mg_http_reply( c, 200, DEFAULT_HEADERS, "" );
    }
    else
    {
      /* bad request the authentification header is not complete/not the correct format */
      mg_http_reply( c, 400, DEFAULT_HEADERS, "" );
    }
  }
  else
  {
    mg_http_reply( c, 404, DEFAULT_HEADERS, "" );
  }
}

static void http_server_get_handler( struct mg_connection* c, struct mg_str* uri, struct mg_http_message* data )
{
  LOG( PRINT_DEBUG, "GET %.*s", uri->len, uri->buf );
  struct mg_str mg_http_root_url = mg_str( http_root_url );
  struct mg_str mg_http_js_url = mg_str( http_js_url );
  struct mg_str mg_http_css_url = mg_str( http_css_url );
  struct mg_str mg_http_ap_url = mg_str( http_ap_url );
  struct mg_str mg_http_status_url = mg_str( http_status_url );

  /* GET /  */
  if ( mg_strcmp( *uri, mg_http_root_url ) == 0 )
  {
    mg_http_reply( c, 200, http_content_type_html, index_html_start );
  }
  /* GET /code.js */
  else if ( mg_strcmp( *uri, mg_http_js_url ) == 0 )
  {
    // mg_http_reply( c, 200, http_content_type_js, code_js_start );
    // Print some statistics about currently established connections
    mg_printf( c, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n" http_content_type_js "\r\n" );
    const char* data = code_js_start;
    do
    {
      size_t size_to_send = code_js_end - data;
      size_t offset = size_to_send > 256 ? 256 : size_to_send;
      mg_http_write_chunk( c, data, offset );
      data += offset;

      if ( size_to_send < 256 )
      {
        break;
      }
    } while ( data < code_js_end );
    mg_http_printf_chunk( c, "" );    // Don't forget the last empty chunk
  }
  /* GET /style.css */
  else if ( mg_strcmp( *uri, mg_http_css_url ) == 0 )
  {
    mg_http_reply( c, 200, http_content_type_css http_cache_control_hdr http_cache_control_cache, style_css_start );
  }
  /* GET /ap.json */
  else if ( mg_strcmp( *uri, mg_http_ap_url ) == 0 )
  {
    /* request a wifi scan */
    wifiDrvStartScanNoBlock();
    /* if we can get the mutex, write the last version of the AP list */
    if ( wifiDrvLockJsonBuffer( 100 ) )
    {
      const char* ap_buf = wifiDrvGetAccessPointsListJson();
      LOG( PRINT_DEBUG, "%s", ap_buf );
      mg_http_reply( c, 200, DEFAULT_HEADERS, ap_buf );
      wifiDrvUnlockJsonBuffer();
    }
    else
    {
      mg_http_reply( c, 503, http_503_hdr, "" );
      LOG( PRINT_ERROR, "http_server_netconn_serve: GET /ap.json failed to obtain mutex" );
    }
  }
  /* GET /status.json */
  else if ( mg_strcmp( *uri, mg_http_status_url ) == 0 )
  {
    if ( wifiDrvLockJsonBuffer( 100 ) )
    {
      const char* buff = wifiDrvGetIpInfoJson();
      LOG( PRINT_DEBUG, "%s", buff );
      if ( buff )
      {
        mg_http_reply( c, 200, DEFAULT_HEADERS, buff );
        wifiDrvUnlockJsonBuffer();
      }
      else
      {
        mg_http_reply( c, 503, http_503_hdr, "" );
      }
    }
    else
    {
      mg_http_reply( c, 503, http_503_hdr, "" );
      LOG( PRINT_ERROR, "http_server_netconn_serve: GET /status.json failed to obtain mutex" );
    }
  }
  else
  {
    mg_http_reply( c, 404, http_404_hdr, "" );
  }
}

/**
 * @brief helper to generate URLs of the wifi manager
 */
static char* http_app_generate_url( const char* page )
{
  char* ret;

  int root_len = strlen( WEBAPP_LOCATION );
  const size_t url_sz = sizeof( char ) * ( ( root_len + 1 ) + ( strlen( page ) + 1 ) );

  ret = malloc( url_sz );
  memset( ret, 0x00, url_sz );
  strcpy( ret, WEBAPP_LOCATION );
  ret = strcat( ret, page );

  return ret;
}

static void fn( struct mg_connection* c, int ev, void* ev_data )
{
  if ( ev == MG_EV_HTTP_MSG )
  {
    struct mg_http_message* hm = (struct mg_http_message*) ev_data;

    if ( mg_strcasecmp( hm->method, mg_str( "GET" ) ) == 0 )
    {
      http_server_get_handler( c, &hm->uri, hm );
    }
    else if ( mg_strcasecmp( hm->method, mg_str( "POST" ) ) == 0 )
    {
      http_server_post_handler( c, &hm->uri, hm );
    }
    else if ( mg_strcasecmp( hm->method, mg_str( "DELETE" ) ) == 0 )
    {
      http_server_delete_handler( c, &hm->uri, hm );
    }
    else
    {
      mg_http_reply( c, 405, "Method Not Allowed\r\n", "" );
    }
  }
}

static void _task( void* argv )
{
  http_app_is_task_run = true;
  LOG( PRINT_INFO, "Init http server" );
  struct mg_mgr mgr;
  mg_mgr_init( &mgr );    // Init manager
  mg_log_set( MG_LL_INFO );    // Set log level
  struct mg_connection* c = mg_http_listen( &mgr, HTTP_WIFI_MANAGER_URL, fn, &mgr );    // Setup listener
  assert( c );
  while ( http_app_is_started == true )
  {
    mg_mgr_poll( &mgr, 1000 );    // Event loop
  }
  mg_mgr_free( &mgr );    // Cleanup
  http_app_is_task_run = false;
  vTaskDelete( NULL );
}

/* Public functions ---------------------------------------------------------*/

void WiFiHTTPApp_Start( void )
{
  assert( http_app_is_started == false );
  /* generate the URLs */
  int root_len = strlen( WEBAPP_LOCATION );

  /* all the pages */
  const char page_js[] = "code.js";
  const char page_css[] = "style.css";
  const char page_connect[] = "connect.json";
  const char page_ap[] = "ap.json";
  const char page_status[] = "status.json";

  /* root url, eg "/"   */
  const size_t http_root_url_sz = sizeof( char ) * ( root_len + 1 );
  http_root_url = malloc( http_root_url_sz );
  memset( http_root_url, 0x00, http_root_url_sz );
  strcpy( http_root_url, WEBAPP_LOCATION );

  /* redirect url */
  size_t redirect_sz = 22 + root_len + 1; /* strlen(http://255.255.255.255) + strlen("/") + 1 for \0 */
  http_redirect_url = malloc( sizeof( char ) * redirect_sz );
  *http_redirect_url = '\0';

  if ( root_len == 1 )
  {
    snprintf( http_redirect_url, redirect_sz, "http://%s", DEFAULT_AP_IP );
  }
  else
  {
    snprintf( http_redirect_url, redirect_sz, "http://%s%s", DEFAULT_AP_IP, WEBAPP_LOCATION );
  }

  /* generate the other pages URLs*/
  http_js_url = http_app_generate_url( page_js );
  http_css_url = http_app_generate_url( page_css );
  http_connect_url = http_app_generate_url( page_connect );
  http_ap_url = http_app_generate_url( page_ap );
  http_status_url = http_app_generate_url( page_status );
  xTaskCreate( _task, "WiFiHTTPApp", 8096, NULL, 13, NULL );
  http_app_is_started = true;
}

void WiFiHTTPApp_Stop( void )
{
  assert( http_app_is_started );
  http_app_is_started = false;
  int cnt = 0;
  while ( http_app_is_task_run )
  {
    assert( cnt < 50 );
    osDelay( 100 );
    cnt++;
  }
  /* dealloc URLs */
  if ( http_root_url )
  {
    free( http_root_url );
    http_root_url = NULL;
  }
  if ( http_redirect_url )
  {
    free( http_redirect_url );
    http_redirect_url = NULL;
  }
  if ( http_js_url )
  {
    free( http_js_url );
    http_js_url = NULL;
  }
  if ( http_css_url )
  {
    free( http_css_url );
    http_css_url = NULL;
  }
  if ( http_connect_url )
  {
    free( http_connect_url );
    http_connect_url = NULL;
  }
  if ( http_ap_url )
  {
    free( http_ap_url );
    http_ap_url = NULL;
  }
  if ( http_status_url )
  {
    free( http_status_url );
    http_status_url = NULL;
  }
}
