#include "mqtt_config.h"

// Mock implementations
bool MQTTConfig_SetInt( int value, mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_SetBool( bool value, mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_SetString( const char* string, mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_SetCert( const char* cert, size_t cert_len, size_t offset, mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_GetInt( int* value, mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_GetBool( bool* value, mqtt_config_value_t config_value )
{
  return false;
}

const char* MQTTConfig_GetString( mqtt_config_value_t config_value )
{
  return false;
}

const char* MQTTConfig_GetCert( mqtt_config_value_t config_value )
{
  return false;
}

bool MQTTConfig_Save( void )
{
  return false;
}

void MQTTConfig_SetCallback( mqtt_apply_config_cb cb )
{
}
