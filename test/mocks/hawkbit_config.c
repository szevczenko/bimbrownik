#include "hawkbit_config.h"

// Mock implementations
bool HAWKBITConfig_SetInt( int value, hawkbit_config_value_t config_value )
{
  return false;
}

bool HAWKBITConfig_SetBool( bool value, hawkbit_config_value_t config_value )
{
  return false;
}

bool HAWKBITConfig_SetString( const char* string, hawkbit_config_value_t config_value )
{
  return false;
}

bool HAWKBITConfig_GetInt( int* value, hawkbit_config_value_t config_value )
{
  return false;
}

bool HAWKBITConfig_GetBool( bool* value, hawkbit_config_value_t config_value )
{
  return false;
}

bool HAWKBITConfig_GetString( char* string, hawkbit_config_value_t config_value, size_t string_len )
{
  return false;
}

bool HAWKBITConfig_Save( void )
{
  return false;
}

void HAWKBITConfig_SetCallback( hawkbit_apply_config_cb cb )
{
}
