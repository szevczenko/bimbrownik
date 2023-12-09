/**
 *******************************************************************************
 * @file    ota_parser.c
 * @author  Dmytro Shevchenko
 * @brief   OTA parser http request
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "ota_parser.h"

#include "app_config.h"
#include "lwjson.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[OTA_P] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private variables ---------------------------------------------------------*/
const char* ota_deployment_action_array[OTA_DEPLOYMENT_ACTION_LAST] =
  {
    [OTA_DEPLOYMENT_ACTION_FORCED] = "forced",
    [OTA_DEPLOYMENT_ACTION_SOFT] = "soft",
    [OTA_DEPLOYMENT_ACTION_DOWNLOAD_ONLY] = "downloadonly",
    [OTA_DEPLOYMENT_ACTION_TIME_FORCED] = "timeforced",
};

/* Private functions ---------------------------------------------------------*/
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
      LOG( PRINT_ERROR, "Buffer to small for copy url" );
      return false;
    }

    memset( string, 0, stringSize );
    memcpy( string, readStr, readStrSize );
    return true;
  }
  return false;
}

static bool _get_url( lwjson_token_t* token, const char* urlName, char* url, size_t urlLen )
{
  if ( token->type == LWJSON_TYPE_OBJECT && 0 == strncmp( urlName, token->token_name, token->token_name_len ) )
  {
    lwjson_token_t* href = (lwjson_token_t*) lwjson_get_first_child( token );
    return _parse_string( href, "href", url, urlLen );
  }
  return false;
}

static ota_deployment_action_t _get_action_type( const char* action )
{
  for ( int i = OTA_DEPLOYMENT_ACTION_UNKNOWN + 1; i < OTA_DEPLOYMENT_ACTION_LAST; i++ )
  {
    if ( memcmp( ota_deployment_action_array[i], action, strlen( ota_deployment_action_array[i] ) ) == 0 )
    {
      return i;
    }
  }
  return OTA_DEPLOYMENT_ACTION_UNKNOWN;
}

static size_t _parse_artifact( lwjson_token_t* chunk_obj_tkn, ota_artifacts_t* artifactArray )
{
  size_t art_index = 0;
  for ( lwjson_token_t* art_tkn = (lwjson_token_t*) lwjson_get_first_child( chunk_obj_tkn ); art_tkn != NULL; art_tkn = art_tkn->next )
  {
    for ( lwjson_token_t* art_obj_tkn = (lwjson_token_t*) lwjson_get_first_child( art_tkn ); art_obj_tkn != NULL; art_obj_tkn = art_obj_tkn->next )
    {
      if ( _parse_string( art_obj_tkn, "filename", artifactArray[art_index].filename, sizeof( artifactArray[art_index].filename ) ) )
      {
        LOG( PRINT_DEBUG, "filename: %s", artifactArray[art_index].filename );
      }
      else if ( art_obj_tkn->type == LWJSON_TYPE_NUM_INT && 0 == strncmp( "size", art_obj_tkn->token_name, art_obj_tkn->token_name_len ) )
      {
        artifactArray[art_index].size = lwjson_get_val_int( art_obj_tkn );
        LOG( PRINT_DEBUG, "size: %d", artifactArray[art_index].size );
      }
      else if ( art_obj_tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "_links", art_obj_tkn->token_name, art_obj_tkn->token_name_len ) )
      {
        for ( lwjson_token_t* url = (lwjson_token_t*) lwjson_get_first_child( art_obj_tkn ); url != NULL; url = url->next )
        {
          _get_url( url, "download-http", artifactArray[art_index].download_http, sizeof( artifactArray[art_index].download_http ) );
        }
      }
    }
    art_index++;
    if ( art_index == OTA_MAX_ARTIFACTS )
    {
      break;
    }
  }
  return art_index;
}

static size_t _parse_chunks( lwjson_token_t* deploymentTkn, ota_chunk_t* chunkArray )
{
  uint8_t index = 0;
  for ( lwjson_token_t* chunk_tkn = (lwjson_token_t*) lwjson_get_first_child( deploymentTkn ); chunk_tkn != NULL; chunk_tkn = chunk_tkn->next )
  {
    for ( lwjson_token_t* chunk_obj_tkn = (lwjson_token_t*) lwjson_get_first_child( chunk_tkn ); chunk_obj_tkn != NULL; chunk_obj_tkn = chunk_obj_tkn->next )
    {
      if ( _parse_string( chunk_obj_tkn, "part", chunkArray[index].part, sizeof( chunkArray[index].part ) ) )
      {
        LOG( PRINT_DEBUG, "part: %s", chunkArray[index].part );
      }
      else if ( _parse_string( chunk_obj_tkn, "version", chunkArray[index].version, sizeof( chunkArray[index].version ) ) )
      {
        LOG( PRINT_DEBUG, "version: %s", chunkArray[index].version );
      }
      else if ( _parse_string( chunk_obj_tkn, "name", chunkArray[index].name, sizeof( chunkArray[index].name ) ) )
      {
        LOG( PRINT_DEBUG, "name: %s", chunkArray[index].name );
      }
      else if ( chunk_obj_tkn->type == LWJSON_TYPE_ARRAY && 0 == strncmp( "artifacts", chunk_obj_tkn->token_name, chunk_obj_tkn->token_name_len ) )
      {
        chunkArray[index].artifactsSize = _parse_artifact( chunk_obj_tkn, chunkArray[index].artifacts );
      }
    }
    index++;
    if ( index == OTA_MAX_CHUNKS )
    {
      break;
    }
  }
  return index;
}

/* Public functions ----------------------------------------------------------*/

bool OTAParser_ParseUrl( const char* jsonString, char* urlConfigData, size_t urlConfigDataSize, char* urlDeploymentBase, size_t urlDeploymentBaseSize )
{
  assert( jsonString );
  assert( urlConfigData );
  assert( urlDeploymentBaseSize );
  assert( urlConfigDataSize );
  assert( urlDeploymentBaseSize );

  if ( strlen( jsonString ) == 0 )
  {
    return false;
  }

  memset( urlConfigData, 0, urlConfigDataSize );
  memset( urlDeploymentBase, 0, urlDeploymentBaseSize );

  lwjson_token_t tokens[16];
  lwjson_t lwjson;
  lwjson_init( &lwjson, tokens, LWJSON_ARRAYSIZE( tokens ) );

  if ( lwjson_parse_ex( &lwjson, jsonString, strlen( jsonString ) ) == lwjsonOK )
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
      if ( tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "_links", tkn->token_name, tkn->token_name_len ) )
      {
        for ( lwjson_token_t* data = (lwjson_token_t*) lwjson_get_first_child( tkn ); data != NULL; data = data->next )
        {
          if ( strlen( urlConfigData ) == 0 && _get_url( data, "configData", urlConfigData, urlConfigDataSize ) )
          {
            continue;
          }
          if ( strlen( urlDeploymentBase ) == 0 && _get_url( data, "deploymentBase", urlDeploymentBase, urlDeploymentBaseSize ) )
          {
            continue;
          }
        }
      }
    }
    lwjson_free( &lwjson );
  }
  else
  {
    lwjson_free( &lwjson );
    return false;
  }

  return true;
}

bool OTAParse_ParseDeployment( const char* jsonString, ota_deployment_t* deployment )
{
  assert( jsonString );
  assert( deployment );

  lwjson_token_t tokens[64];
  lwjson_t lwjson;
  lwjson_init( &lwjson, tokens, LWJSON_ARRAYSIZE( tokens ) );

  if ( lwjson_parse_ex( &lwjson, jsonString, strlen( jsonString ) ) == lwjsonOK )
  {
    lwjson_token_t* t;

    /* Get first token as top object */
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
      if ( tkn->type == LWJSON_TYPE_OBJECT && 0 == strncmp( "deployment", tkn->token_name, tkn->token_name_len ) )
      {
        /* Deployment */
        for ( lwjson_token_t* deploymentTkn = (lwjson_token_t*) lwjson_get_first_child( tkn ); deploymentTkn != NULL; deploymentTkn = deploymentTkn->next )
        {
          if ( deploymentTkn->type == LWJSON_TYPE_STRING )
          {
            char string[32] = {};
            if ( _parse_string( deploymentTkn, "update", string, sizeof( string ) ) )
            {
              LOG( PRINT_DEBUG, "update: %s", string );
              deployment->update = _get_action_type( string );
              continue;
            }
            else if ( _parse_string( deploymentTkn, "download", string, sizeof( string ) ) )
            {
              LOG( PRINT_DEBUG, "download: %s", string );
              deployment->download = _get_action_type( string );
              continue;
            }
          }
          else if ( deploymentTkn->type == LWJSON_TYPE_ARRAY && 0 == strncmp( "chunks", deploymentTkn->token_name, deploymentTkn->token_name_len ) )
          {
            deployment->chunkSize = _parse_chunks( deploymentTkn, deployment->chunk );
          }
        }
      }
    }
    lwjson_free( &lwjson );
  }
  else
  {
    lwjson_free( &lwjson );
    return false;
  }
  return true;
}
