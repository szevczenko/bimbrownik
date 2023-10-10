#include <string.h>

#include "wifi.h"

wifi_err_t WiFiInitAccessPoint( const char* name )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiInitSTA( void )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiDeinit( void )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiConnect( const char* ap_name, const char* password )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiConnectLastData( void )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiDisconnect( void )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiStop( void )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiReadInfo( int8_t* rssi )
{
  return WIFI_ERR_OK;
}

wifi_err_t wifiDataSave( const wifiConData_t* data )
{
  return WIFI_ERR_OK;
}

wifi_err_t wifiDataRead( wifiConData_t* data )
{
  strcpy( data->ssid, "exampleSsid" );
  strcpy( data->password, "examplePassword" );
  return WIFI_ERR_OK;
}

wifi_err_t WiFiWPSStart( uint32_t time_ms )
{
  return WIFI_ERR_OK;
}

wifi_err_t WiFiWPSStop( void )
{
  return WIFI_ERR_OK;
}