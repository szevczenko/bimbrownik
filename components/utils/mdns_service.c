/**
 *******************************************************************************
 * @file    mdns_service.c
 * @author  Dmytro Shevchenko
 * @brief   mDNS service for indetification device
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "mdns_service.h"

#include "app_config.h"
#include "esp_mac.h"
#include "mdns.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[mDNS Srv] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_TCP_SERVER
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define MB_MDNS_PORT      ( 5353 )
#define MB_ID_BYTE0( id ) ( (uint8_t) ( id ) )
#define MB_ID_BYTE1( id ) ( (uint8_t) ( ( (uint16_t) ( id ) >> 8 ) & 0xFF ) )
#define MB_ID_BYTE2( id ) ( (uint8_t) ( ( (uint32_t) ( id ) >> 16 ) & 0xFF ) )
#define MB_ID_BYTE3( id ) ( (uint8_t) ( ( (uint32_t) ( id ) >> 24 ) & 0xFF ) )
#define MB_ID2STR( id )   MB_ID_BYTE0( id ), MB_ID_BYTE1( id ), MB_ID_BYTE2( id ), MB_ID_BYTE3( id )
#define MB_DEVICE_ID      (uint32_t) 0x00112233

/* Private functions ---------------------------------------------------------*/
static inline char* gen_mac_str( const uint8_t* mac, char* pref, char* mac_str )
{
  sprintf( mac_str, "%s%02X%02X%02X%02X%02X%02X", pref, MAC2STR( mac ) );
  return mac_str;
}

/* Public functions ---------------------------------------------------------*/
void mDNS_Start( void )
{
  char temp_str[32] = { 0 };
  uint8_t sta_mac[6] = { 0 };
  ESP_ERROR_CHECK( esp_read_mac( sta_mac, ESP_MAC_WIFI_STA ) );
  char* hostname = "Bimbrownik";
  //initialize mDNS
  ESP_ERROR_CHECK( mdns_init() );
  //set mDNS hostname (required if you want to advertise services)
  ESP_ERROR_CHECK( mdns_hostname_set( hostname ) );
  LOG( PRINT_INFO, "mdns hostname set to: [%s]", hostname );

  //set default mDNS instance name
  ESP_ERROR_CHECK( mdns_instance_name_set( "bimbrownik" ) );

  //structure with TXT records
  mdns_txt_item_t serviceTxtData[] = {
    {"board", "esp32"}
  };

  //initialize service
  ESP_ERROR_CHECK( mdns_service_add( "Bimbrownik TCP Server", "_remote", "_tcp", DEV_CONFIG_TCP_SERVER_PORT, serviceTxtData, 1 ) );
  //add mac key string text item
  ESP_ERROR_CHECK( mdns_service_txt_item_set( "_remote", "_tcp", "mac", gen_mac_str( sta_mac, "\0", temp_str ) ) );
}

void mDNS_Stop( void )
{
  mdns_free();
}