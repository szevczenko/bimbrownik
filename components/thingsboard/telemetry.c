#include "telemetry.h"

#include <stdio.h>
#include <string.h>

#include "mongoose.h"

static telemetry_key_t* find_key( telemetry_t* tel, const char* key )
{
  if ( !tel || !key )
    return NULL;

  telemetry_key_t* current = tel->keys;
  while ( current )
  {
    if ( strcmp( current->key, key ) == 0 )
    {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static void free_key_data( telemetry_key_t* key )
{
  if ( !key )
    return;

  switch ( key->type )
  {
    case TELEMETRY_TYPE_STRING:
    case TELEMETRY_TYPE_JSON:
      free( key->data.string_val );
      break;
    default:
      break;
  }
}

static telemetry_key_t* create_key( const char* key_name, telemetry_key_type_t type )
{
  telemetry_key_t* new_key = calloc( 1, sizeof( telemetry_key_t ) );
  if ( !new_key )
    return NULL;

  new_key->key = strdup( key_name );
  if ( !new_key->key )
  {
    free( new_key );
    return NULL;
  }

  new_key->type = type;
  new_key->next = NULL;
  return new_key;
}

telemetry_t* telemetry_create( void )
{
  telemetry_t* tel = calloc( 1, sizeof( telemetry_t ) );
  if ( !tel )
    return NULL;

  tel->keys = NULL;
  tel->count = 0;
  return tel;
}

static int add_or_update_key( telemetry_t* tel, const char* key, telemetry_key_type_t type, telemetry_key_data_t data )
{
  if ( !tel || !key )
    return -1;

  telemetry_key_t* existing = find_key( tel, key );
  if ( existing )
  {
    // Update existing key
    free_key_data( existing );
    existing->type = type;
    existing->data = data;
    return 0;
  }

  // Create new key
  telemetry_key_t* new_key = create_key( key, type );
  if ( !new_key )
    return -1;

  new_key->data = data;
  new_key->next = tel->keys;
  tel->keys = new_key;
  tel->count++;

  return 0;
}

int telemetry_add_string( telemetry_t* tel, const char* key, const char* value )
{
  if ( !value )
    return -1;

  telemetry_key_data_t data;
  data.string_val = strdup( value );
  if ( !data.string_val )
    return -1;

  return add_or_update_key( tel, key, TELEMETRY_TYPE_STRING, data );
}

int telemetry_add_boolean( telemetry_t* tel, const char* key, bool value )
{
  telemetry_key_data_t data;
  data.bool_val = value;
  return add_or_update_key( tel, key, TELEMETRY_TYPE_BOOLEAN, data );
}

int telemetry_add_double( telemetry_t* tel, const char* key, double value )
{
  telemetry_key_data_t data;
  data.double_val = value;
  return add_or_update_key( tel, key, TELEMETRY_TYPE_DOUBLE, data );
}

int telemetry_add_long( telemetry_t* tel, const char* key, long value )
{
  telemetry_key_data_t data;
  data.long_val = value;
  return add_or_update_key( tel, key, TELEMETRY_TYPE_LONG, data );
}

int telemetry_add_json( telemetry_t* tel, const char* key, const char* json_value )
{
  if ( !json_value )
    return -1;

  telemetry_key_data_t data;
  data.json_val = strdup( json_value );
  if ( !data.json_val )
    return -1;

  return add_or_update_key( tel, key, TELEMETRY_TYPE_JSON, data );
}

char* telemetry_to_json( telemetry_t* tel )
{
  if ( !tel )
    return NULL;

  char* json = NULL;

  // Start with opening brace
  json = mg_mprintf( "{" );
  if ( !json )
    return NULL;

  telemetry_key_t* current = tel->keys;
  bool first = true;

  while ( current )
  {
    char* temp;

    if ( !first )
    {
      temp = mg_mprintf( "%s,", json );  // Fixed spacing for cleaner JSON
      free( json );
      json = temp;
    }
    else
    {
      first = false;
    }

    switch ( current->type )
    {
      case TELEMETRY_TYPE_STRING:
        temp = mg_mprintf( "%s\"%s\":\"%s\"", json, current->key, current->data.string_val );
        break;

      case TELEMETRY_TYPE_BOOLEAN:
        temp = mg_mprintf( "%s\"%s\":%s", json, current->key, 
                           current->data.bool_val ? "true" : "false" );
        break;

      case TELEMETRY_TYPE_DOUBLE:
        temp = mg_mprintf( "%s\"%s\":%.6f", json, current->key, current->data.double_val );
        break;

      case TELEMETRY_TYPE_LONG:
        temp = mg_mprintf( "%s\"%s\":%ld", json, current->key, current->data.long_val );
        break;

      case TELEMETRY_TYPE_JSON:
        temp = mg_mprintf( "%s\"%s\":%s", json, current->key, current->data.json_val );
        break;

      default:
        temp = json;
        break;
    }

    if ( temp != json )
    {
      free( json );
      json = temp;
    }

    current = current->next;
  }

  // Close the JSON object
  char* final_json = mg_mprintf( "%s}", json );
  free( json );

  return final_json;
}

void telemetry_free( telemetry_t* tel )
{
  if ( !tel )
    return;

  telemetry_key_t* current = tel->keys;
  while ( current )
  {
    telemetry_key_t* next = current->next;

    free( current->key );
    free_key_data( current );
    free( current );

    current = next;
  }

  free( tel );
}
