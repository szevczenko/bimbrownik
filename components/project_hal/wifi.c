/**
 *******************************************************************************
 * @file    wifi.c
 * @author  Dmytro Shevchenko
 * @brief   Wifi Hal Layer
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "wifi.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_wps.h"
#include "netif/ethernet.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[WiFiHal] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define WIFI_NVS_STORAGE_NAME  "WiFiStorage"
#define DEFAULT_SCAN_LIST_SIZE 32
#define MAX_VAL( a, b )        a > b ? a : b
#define WIFI_ERROR_CHECK( _func ) \
  if ( ( _func ) != ESP_OK )      \
  {                               \
    return WIFI_ERR_FAIL;         \
  }

/* Private types -------------------------------------------------------------*/
typedef struct
{
  int retry;
  uint32_t connect_attemps;
  uint32_t reason_disconnect;
  wifi_ap_record_t scan_list[DEFAULT_SCAN_LIST_SIZE];
  wifi_config_t wifi_config;
  wifiConData_t wifi_con_data;
  esp_netif_t* netif;
  int rssi;
} wifi_ctx_t;

/* Private variables ---------------------------------------------------------*/
static wifi_ctx_t ctx;

static wifi_config_t wifi_config_ap = {
  .ap = {
         .max_connection = 2,
         .authmode = WIFI_AUTH_WPA_WPA2_PSK},
};

/* Private functions ---------------------------------------------------------*/
static void _wifi_read_info_cb( void* arg, wifi_vendor_ie_type_t type, const uint8_t sa[6],
                                const vendor_ie_data_t* vnd_ie, int rssi )
{
}

static void _on_wifi_disconnect_cb( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
  system_event_sta_disconnected_t* event = (system_event_sta_disconnected_t*) event_data;

  ctx.reason_disconnect = event->reason;
  /* Is needed ??? */
  tcpip_adapter_down( TCPIP_ADAPTER_IF_STA );
}

static void _wifi_scan_done_cb( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
  switch ( event_id )
  {
    case WIFI_EVENT_SCAN_DONE:
      LOG( PRINT_INFO, "SCAN DONE!!!!!!" );
      break;
    default:
      break;
  }
}

static void _got_ip_event_cb( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
  switch ( event_id )
  {
    case IP_EVENT_STA_GOT_IP:
      LOG( PRINT_INFO, "%s Have IP", __func__, event_base, event_id );
      if ( ( memcmp( ctx.wifi_con_data.ssid, ctx.wifi_config.sta.ssid,
                     MAX_VAL( strlen( (char*) ctx.wifi_config.sta.ssid ), strlen( (char*) ctx.wifi_con_data.ssid ) ) )
             != 0 )
           || ( memcmp( ctx.wifi_con_data.password, ctx.wifi_config.sta.password,
                        MAX_VAL( strlen( (char*) ctx.wifi_config.sta.password ), strlen( (char*) ctx.wifi_con_data.password ) ) )
                != 0 ) )
      {
        strncpy( (char*) ctx.wifi_con_data.ssid, (char*) ctx.wifi_config.sta.ssid, sizeof( ctx.wifi_con_data.ssid ) );
        strncpy( (char*) ctx.wifi_con_data.password, (char*) ctx.wifi_config.sta.password,
                 sizeof( ctx.wifi_con_data.password ) );
        /* wifiDataSave( &ctx.wifi_con_data ); */
      }
      break;
  }
}

// static void _wps_event_handler( void* arg, esp_event_base_t event_base,
//                                 int32_t event_id, void* event_data )
// {
//   static int ap_idx = 1;

//   switch ( event_id )
//   {
//     case WIFI_EVENT_STA_WPS_ER_SUCCESS:
//       // ESP_LOGI( TAG, "WIFI_EVENT_STA_WPS_ER_SUCCESS" );
//       {
//         wifi_event_sta_wps_er_success_t* evt =
//           (wifi_event_sta_wps_er_success_t*) event_data;
//         int i;

//         if ( evt )
//         {
//           s_ap_creds_num = evt->ap_cred_cnt;
//           for ( i = 0; i < s_ap_creds_num; i++ )
//           {
//             memcpy( wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
//                     sizeof( evt->ap_cred[i].ssid ) );
//             memcpy( wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
//                     sizeof( evt->ap_cred[i].passphrase ) );
//           }
//           /* If multiple AP credentials are received from WPS, connect with first one */
//           // ESP_LOGI( TAG, "Connecting to SSID: %s, Passphrase: %s",
//           //           wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password );
//           ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &wps_ap_creds[0] ) );
//         }
//        /*
//         * If only one AP credential is received from WPS, there will be no event data and
//         * esp_wifi_set_config() is already called by WPS modules for backward compatibility
//         * with legacy apps. So directly attempt connection here.
//         */
//         ESP_ERROR_CHECK( esp_wifi_wps_disable() );
//         esp_wifi_connect();
//       }
//       break;
//     case WIFI_EVENT_STA_WPS_ER_FAILED:
//       // ESP_LOGI( TAG, "WIFI_EVENT_STA_WPS_ER_FAILED" );
//       ESP_ERROR_CHECK( esp_wifi_wps_disable() );
//       ESP_ERROR_CHECK( esp_wifi_wps_enable( &config ) );
//       ESP_ERROR_CHECK( esp_wifi_wps_start( 0 ) );
//       break;
//     case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
//       // ESP_LOGI( TAG, "WIFI_EVENT_STA_WPS_ER_TIMEOUT" );
//       ESP_ERROR_CHECK( esp_wifi_wps_disable() );
//       ESP_ERROR_CHECK( esp_wifi_wps_enable( &config ) );
//       ESP_ERROR_CHECK( esp_wifi_wps_start( 0 ) );
//       break;
//     case WIFI_EVENT_STA_WPS_ER_PIN:
//       // ESP_LOGI( TAG, "WIFI_EVENT_STA_WPS_ER_PIN" );
//       /* display the PIN code */
//       wifi_event_sta_wps_er_pin_t* event = (wifi_event_sta_wps_er_pin_t*) event_data;
//       // ESP_LOGI( TAG, "WPS_PIN = " PINSTR, PIN2STR( event->pin_code ) );
//       break;
//     default:
//       break;
//   }
// }

static void _debug_cb( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data )
{
  LOG( PRINT_DEBUG, "%s EVENT_WIFI %s %d", __func__, event_base, event_id );
  if ( event_id == WIFI_EVENT_STA_DISCONNECTED )
  {
    wifi_event_sta_disconnected_t* data = event_data;
    LOG( PRINT_DEBUG, "Ssid %s bssid %x.%x.%x.%x.%x.%x len %d reason %d/n/r", data->ssid, data->bssid[0],
         data->bssid[1], data->bssid[2], data->bssid[3], data->bssid[4], data->bssid[5], data->ssid_len,
         data->reason );
  }
}

static void _print_auth_mode( int authmode )
{
  switch ( authmode )
  {
    case WIFI_AUTH_OPEN:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_OPEN" );
      break;
    case WIFI_AUTH_WEP:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WEP" );
      break;
    case WIFI_AUTH_WPA_PSK:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA_PSK" );
      break;
    case WIFI_AUTH_WPA2_PSK:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA2_PSK" );
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK" );
      break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE" );
      break;
    case WIFI_AUTH_WPA3_PSK:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA3_PSK" );
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK" );
      break;
    default:
      LOG( PRINT_DEBUG, "Authmode \tWIFI_AUTH_UNKNOWN" );
      break;
  }
}

static void _print_cipher_type( int pairwise_cipher, int group_cipher )
{
  switch ( pairwise_cipher )
  {
    case WIFI_CIPHER_TYPE_NONE:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE" );
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40" );
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104" );
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP" );
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP" );
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP" );
      break;
    default:
      LOG( PRINT_DEBUG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN" );
      break;
  }

  switch ( group_cipher )
  {
    case WIFI_CIPHER_TYPE_NONE:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE" );
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40" );
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104" );
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP" );
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP" );
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP" );
      break;
    default:
      LOG( PRINT_DEBUG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN" );
      break;
  }
}

/* Public functions ---------------------------------------------------------*/

wifi_err_t WiFiInitAccessPoint( const char* name )
{
  /* Nadawanie nazwy WiFi Access point oraz przypisanie do niego mac adresu */
  uint8_t mac[6];
  esp_efuse_mac_get_default( mac );
  strcpy( (char*) wifi_config_ap.ap.ssid, name );
  for ( int i = 0; i < sizeof( mac ); i++ )
  {
    sprintf( (char*) &wifi_config_ap.ap.ssid[strlen( (char*) wifi_config_ap.ap.ssid )], ":%x", mac[i] );
  }

  wifi_config_ap.ap.ssid_len = strlen( (char*) wifi_config_ap.ap.ssid );

  ctx.wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  ctx.wifi_config.sta.pmf_cfg.capable = true;

  /* Inicjalizacja WiFi */
  WIFI_ERROR_CHECK( esp_netif_init() );
  WIFI_ERROR_CHECK( esp_event_loop_create_default() );
  ctx.netif = esp_netif_create_default_wifi_ap();

  if ( ctx.netif == NULL )
  {
    return WIFI_ERR_FAIL;
  }
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  WIFI_ERROR_CHECK( esp_wifi_init( &cfg ) );

  WIFI_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_AP ) );
  WIFI_ERROR_CHECK( esp_wifi_set_config( ESP_IF_WIFI_AP, &wifi_config_ap ) );
  WIFI_ERROR_CHECK( esp_wifi_start() );

  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_wifi_disconnect_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &_wifi_scan_done_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( IP_EVENT, IP_EVENT_STA_GOT_IP, &_got_ip_event_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_MASK_ALL, &_debug_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( IP_EVENT, WIFI_EVENT_MASK_ALL, &_debug_cb, NULL ) );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiInitSTA( void )
{
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ctx.wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  ctx.wifi_config.sta.pmf_cfg.capable = true;

  /* Inicjalizacja WiFi */
  if ( esp_netif_init() != ESP_OK )
  {
    return WIFI_ERR_FAIL;
  }
  WIFI_ERROR_CHECK( esp_event_loop_create_default() );
  ctx.netif = esp_netif_create_default_wifi_sta();

  if ( ctx.netif == NULL )
  {
    return WIFI_ERR_FAIL;
  }
  WIFI_ERROR_CHECK( esp_wifi_init( &cfg ) );
  WIFI_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &ctx.wifi_config ) );
  WIFI_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
  WIFI_ERROR_CHECK( esp_wifi_start() );
  WIFI_ERROR_CHECK( esp_wifi_set_vendor_ie_cb( _wifi_read_info_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &_on_wifi_disconnect_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &_wifi_scan_done_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( IP_EVENT, IP_EVENT_STA_GOT_IP, &_got_ip_event_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_MASK_ALL, &_debug_cb, NULL ) );
  WIFI_ERROR_CHECK( esp_event_handler_register( IP_EVENT, WIFI_EVENT_MASK_ALL, &_debug_cb, NULL ) );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiConnect( const char* ap_name, const char* password )
{
  if ( ap_name == NULL || password == NULL )
  {
    return WIFI_ERR_INVALID_ARG;
  }
  memset( ctx.wifi_config.sta.ssid, 0, sizeof( ctx.wifi_config.sta.ssid ) );
  memset( ctx.wifi_config.sta.password, 0, sizeof( ctx.wifi_config.sta.password ) );
  strncpy( (char*) ctx.wifi_config.sta.ssid, ap_name, sizeof( ctx.wifi_config.sta.ssid ) );
  strncpy( (char*) ctx.wifi_config.sta.password, password, sizeof( ctx.wifi_config.sta.password ) );

  WIFI_ERROR_CHECK( esp_wifi_set_config( ESP_IF_WIFI_STA, &ctx.wifi_config ) );
  WIFI_ERROR_CHECK( esp_wifi_connect() );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiDisconnect( void )
{
  WIFI_ERROR_CHECK( esp_wifi_disconnect() );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiStop( void )
{
  WIFI_ERROR_CHECK( esp_wifi_disconnect() );
  WIFI_ERROR_CHECK( esp_wifi_stop() );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiConnectLastData( void )
{
  WIFI_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
  WIFI_ERROR_CHECK( esp_wifi_start() );
  WIFI_ERROR_CHECK( esp_wifi_set_vendor_ie_cb( _wifi_read_info_cb, NULL ) );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiReadInfo( int8_t* rssi )
{
  wifi_ap_record_t ap_info = { 0 };
  WIFI_ERROR_CHECK( esp_wifi_sta_get_ap_info( &ap_info ) );
  ctx.rssi = ap_info.rssi;
  *rssi = ap_info.rssi;
  return WIFI_ERR_OK;
}

wifi_err_t WiFiDeinit( void )
{
  esp_wifi_wps_disable();
  esp_wifi_disconnect();
  esp_wifi_stop();
  WIFI_ERROR_CHECK( esp_wifi_deinit() );
  return WIFI_ERR_OK;
}

wifi_err_t wifiDrvGetScanResult( uint16_t* ap_count )
{
  uint16_t number = DEFAULT_SCAN_LIST_SIZE;

  WIFI_ERROR_CHECK( esp_wifi_scan_get_ap_records( &number, ctx.scan_list ) );
  WIFI_ERROR_CHECK( esp_wifi_scan_get_ap_num( ap_count ) );
  for ( uint32_t i = 0; i < *ap_count; i++ )
  {
    LOG( PRINT_DEBUG, "AP: %s CH %d CH2 %d RSSI %d", ctx.scan_list[i].ssid, ctx.scan_list[i].primary,
         ctx.scan_list[i].second, ctx.scan_list[i].rssi );
    _print_auth_mode( ctx.scan_list[i].authmode );
    if ( ctx.scan_list[i].authmode != WIFI_AUTH_WEP )
    {
      _print_cipher_type( ctx.scan_list[i].pairwise_cipher, ctx.scan_list[i].group_cipher );
    }
  }
  return WIFI_ERR_OK;
}

wifi_err_t wifiDrvGetNameFromScannedList( uint8_t number, char* name )
{
  if ( number >= DEFAULT_SCAN_LIST_SIZE )
  {
    return WIFI_ERR_INVALID_ARG;
  }
  strcpy( name, (char*) ctx.scan_list[number].ssid );
  return WIFI_ERR_OK;
}

wifi_err_t wifiDataSave( const wifiConData_t* data )
{
  nvs_handle my_handle;
  esp_err_t err;

  // Open
  err = nvs_open( WIFI_NVS_STORAGE_NAME, NVS_READWRITE, &my_handle );
  if ( err != ESP_OK )
  {
    nvs_close( my_handle );
    return WIFI_ERR_FAIL;
  }

  err = nvs_set_blob( my_handle, "wifi", data, sizeof( wifiConData_t ) );

  if ( err != ESP_OK )
  {
    nvs_close( my_handle );
    return WIFI_ERR_FAIL;
  }

  // Commit
  err = nvs_commit( my_handle );
  if ( err != ESP_OK )
  {
    nvs_close( my_handle );
    return WIFI_ERR_FAIL;
  }

  // Close
  nvs_close( my_handle );
  return WIFI_ERR_OK;
}

wifi_err_t wifiDataRead( wifiConData_t* data )
{
  nvs_handle my_handle;
  esp_err_t err;

  // Open
  err = nvs_open( WIFI_NVS_STORAGE_NAME, NVS_READWRITE, &my_handle );
  if ( err != ESP_OK )
  {
    return WIFI_ERR_FAIL;
  }

  // Read the size of memory space required for blob
  size_t required_size = 0;    // value will default to 0, if not set yet in NVS

  err = nvs_get_blob( my_handle, "wifi", NULL, &required_size );
  if ( ( err != ESP_OK ) && ( err != ESP_ERR_NVS_NOT_FOUND ) )
  {
    nvs_close( my_handle );
    return WIFI_ERR_FAIL;
  }

  // Read previously saved blob if available
  if ( required_size == sizeof( wifiConData_t ) )
  {
    err = nvs_get_blob( my_handle, "wifi", data, &required_size );
    nvs_close( my_handle );
    return WIFI_ERR_OK;
  }

  nvs_close( my_handle );
  return WIFI_ERR_FAIL;
}

wifi_err_t WiFiWPSStart( uint32_t time_ms )
{
  esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT( WPS_TYPE_PBC );
  WIFI_ERROR_CHECK( esp_wifi_wps_enable( &config ) );
  WIFI_ERROR_CHECK( esp_wifi_wps_start( time_ms ) );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiWPSStop( void )
{
  WIFI_ERROR_CHECK( esp_wifi_wps_disable() );
  return WIFI_ERR_OK;
}