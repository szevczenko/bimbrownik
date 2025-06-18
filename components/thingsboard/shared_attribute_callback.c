#include "shared_attribute_callback.h"

#include <stdio.h>
#include <string.h>

#include "mongoose.h"

bool shared_attribute_callback_init( shared_attribute_callback_t* callback_obj )
{
  if ( !callback_obj )
  {
    return false;
  }

  memset( callback_obj, 0, sizeof( shared_attribute_callback_t ) );
  callback_obj->attribute_count = 0;
  callback_obj->callback = NULL;
  callback_obj->active = false;

  return true;
}

bool shared_attribute_callback_set_function( shared_attribute_callback_t* callback_obj,
                                             shared_attribute_callback_fn_t callback )
{
  if ( !callback_obj || !callback )
  {
    return false;
  }

  callback_obj->callback = callback;
  callback_obj->active = ( callback != NULL );

  return true;
}

bool shared_attribute_callback_add_attribute( shared_attribute_callback_t* callback_obj,
                                              const char* attribute_name )
{
  if ( !callback_obj || !attribute_name )
  {
    return false;
  }

  if ( callback_obj->attribute_count >= MAX_SHARED_ATTRIBUTES )
  {
    printf( "Maximum number of shared attributes reached\n" );
    return false;
  }

  if ( strlen( attribute_name ) >= MAX_ATTRIBUTE_NAME_LENGTH )
  {
    printf( "Attribute name too long: %s\n", attribute_name );
    return false;
  }

  // Check if attribute already exists
  for ( size_t i = 0; i < callback_obj->attribute_count; i++ )
  {
    if ( strcmp( callback_obj->attributes[i], attribute_name ) == 0 )
    {
      // Attribute already exists, no need to add again
      return true;
    }
  }

  // Add new attribute
  strncpy( callback_obj->attributes[callback_obj->attribute_count],
           attribute_name,
           MAX_ATTRIBUTE_NAME_LENGTH - 1 );
  callback_obj->attributes[callback_obj->attribute_count][MAX_ATTRIBUTE_NAME_LENGTH - 1] = '\0';
  callback_obj->attribute_count++;

  return true;
}

bool shared_attribute_callback_set_attributes( shared_attribute_callback_t* callback_obj,
                                               const char* const* attribute_names,
                                               size_t count )
{
  if ( !callback_obj || !attribute_names )
  {
    return false;
  }

  if ( count > MAX_SHARED_ATTRIBUTES )
  {
    printf( "Too many attributes requested: %zu (max: %d)\n", count, MAX_SHARED_ATTRIBUTES );
    return false;
  }

  // Clear existing attributes
  shared_attribute_callback_clear_attributes( callback_obj );

  // Add new attributes
  for ( size_t i = 0; i < count; i++ )
  {
    if ( !shared_attribute_callback_add_attribute( callback_obj, attribute_names[i] ) )
    {
      return false;
    }
  }

  return true;
}

size_t shared_attribute_callback_get_attributes( const shared_attribute_callback_t* callback_obj,
                                                 const char** attributes,
                                                 size_t max_count )
{
  if ( !callback_obj || !attributes )
  {
    return 0;
  }

  size_t count = callback_obj->attribute_count < max_count ?
                   callback_obj->attribute_count :
                   max_count;

  for ( size_t i = 0; i < count; i++ )
  {
    attributes[i] = callback_obj->attributes[i];
  }

  return count;
}

size_t shared_attribute_callback_get_count( const shared_attribute_callback_t* callback_obj )
{
  if ( !callback_obj )
  {
    return 0;
  }

  return callback_obj->attribute_count;
}

bool shared_attribute_callback_has_attribute( const shared_attribute_callback_t* callback_obj,
                                              const char* attribute_name )
{
  if ( !callback_obj || !attribute_name )
  {
    return false;
  }

  for ( size_t i = 0; i < callback_obj->attribute_count; i++ )
  {
    if ( strcmp( callback_obj->attributes[i], attribute_name ) == 0 )
    {
      return true;
    }
  }

  return false;
}

void shared_attribute_callback_clear_attributes( shared_attribute_callback_t* callback_obj )
{
  if ( !callback_obj )
  {
    return;
  }

  memset( callback_obj->attributes, 0, sizeof( callback_obj->attributes ) );
  callback_obj->attribute_count = 0;
}

bool shared_attribute_callback_is_empty( const shared_attribute_callback_t* callback_obj )
{
  if ( !callback_obj )
  {
    return true;
  }

  return callback_obj->attribute_count == 0;
}

bool shared_attribute_callback_process_data( const shared_attribute_callback_t* callback_obj,
                                             const char* json_data,
                                             size_t data_len )
{
  if ( !callback_obj || !json_data || !callback_obj->active || !callback_obj->callback )
  {
    return false;
  }

  if ( callback_obj->attribute_count == 0 )
  {
    return false;
  }

  // Parse JSON to check if any of our monitored attributes are present
  struct mg_str json_str = mg_str_n( json_data, data_len );
  bool has_monitored_attribute = false;

  for ( size_t i = 0; i < callback_obj->attribute_count; i++ )
  {
    char path[128];
    snprintf( path, sizeof( path ), "$.%s", callback_obj->attributes[i] );

    int token_len;
    int offset = mg_json_get( json_str, path, &token_len );

    if ( offset >= 0 )
    {
      has_monitored_attribute = true;
      break;
    }
  }

  if ( has_monitored_attribute )
  {
    callback_obj->callback( json_data, data_len );
    return true;
  }

  return false;
}

void shared_attribute_callback_cleanup( shared_attribute_callback_t* callback_obj )
{
  if ( !callback_obj )
  {
    return;
  }

  shared_attribute_callback_clear_attributes( callback_obj );
  callback_obj->callback = NULL;
  callback_obj->active = false;
}
