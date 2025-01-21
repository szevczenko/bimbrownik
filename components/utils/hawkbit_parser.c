/**
 *******************************************************************************
 * @file    hawkbit_parser.c
 * @author  Dmytro Shevchenko
 * @brief   HAWKBIT parser http request
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hawkbit_parser.h"

#include "app_config.h"
#include "mongoose.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[HAWKBIT_P] "
#define DEBUG_LVL   PRINT_DEBUG

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private variables ---------------------------------------------------------*/
const char* hawkbit_deployment_action_array[HAWKBIT_DEPLOYMENT_ACTION_LAST] =
  {
    [HAWKBIT_DEPLOYMENT_ACTION_FORCED] = "forced",
    [HAWKBIT_DEPLOYMENT_ACTION_SOFT] = "soft",
    [HAWKBIT_DEPLOYMENT_ACTION_DOWNLOAD_ONLY] = "downloadonly",
    [HAWKBIT_DEPLOYMENT_ACTION_TIME_FORCED] = "timeforced",
};

const char* hawkbit_execution_status_array[HAWKBIT_EXECUTION_STATUS_MAX] =
  {
    [HAWKBIT_EXECUTION_STATUS_CANCELED] = "canceled",
    [HAWKBIT_EXECUTION_STATUS_REJECTED] = "rejected",
    [HAWKBIT_EXECUTION_STATUS_CLOSED] = "closed",
    [HAWKBIT_EXECUTION_STATUS_PROCEEDING] = "proceeding",
    [HAWKBIT_EXECUTION_STATUS_SCHEDULED] = "scheduled",
    [HAWKBIT_EXECUTION_STATUS_RESUMED] = "resumed",
};

/* Private functions ---------------------------------------------------------*/

static hawkbit_deployment_action_t _get_action_type( const char* action )
{
  for ( int i = HAWKBIT_DEPLOYMENT_ACTION_UNKNOWN + 1; i < HAWKBIT_DEPLOYMENT_ACTION_LAST; i++ )
  {
    if ( memcmp( hawkbit_deployment_action_array[i], action, strlen( hawkbit_deployment_action_array[i] ) ) == 0 )
    {
      return i;
    }
  }
  return HAWKBIT_DEPLOYMENT_ACTION_UNKNOWN;
}

/* Public functions ----------------------------------------------------------*/

bool HAWKBITParser_ParseUrl( const char* jsonString, char* urlConfigData, size_t urlConfigDataSize, char* urlDeploymentBase, size_t urlDeploymentBaseSize, char* urlCancelAction, size_t urlCancelActionSize )
{
  assert( jsonString );
  assert( urlConfigData );
  assert( urlDeploymentBase );
  assert( urlCancelAction );
  assert( urlConfigDataSize );
  assert( urlDeploymentBaseSize );
  assert( urlCancelActionSize );

  if ( strlen( jsonString ) == 0 )
  {
    return false;
  }

  memset( urlConfigData, 0, urlConfigDataSize );
  memset( urlDeploymentBase, 0, urlDeploymentBaseSize );
  memset( urlCancelAction, 0, urlCancelActionSize );

  struct mg_str json = mg_str( jsonString );
  int len;
  int offset = mg_json_get( json, "$._links", &len );

  if ( offset < 0 )
  {
    LOG( PRINT_ERROR, "Invalid JSON: missing _links" );
    return false;
  }

  struct mg_str links = mg_str_n( json.buf + offset, len );

  offset = mg_json_get( links, "$.configData.href", &len );
  if ( offset >= 0 )
  {
    snprintf( urlConfigData, urlConfigDataSize, "%.*s", len, links.buf + offset );
  }

  offset = mg_json_get( links, "$.deploymentBase.href", &len );
  if ( offset >= 0 )
  {
    snprintf( urlDeploymentBase, urlDeploymentBaseSize, "%.*s", len, links.buf + offset );
  }

  offset = mg_json_get( links, "$.cancelAction.href", &len );
  if ( offset >= 0 )
  {
    snprintf( urlCancelAction, urlCancelActionSize, "%.*s", len, links.buf + offset );
  }

  return true;
}

bool HAWKBITParse_ParseDeployment( const char* jsonString, hawkbit_deployment_t* deployment )
{
  assert( jsonString );
  assert( deployment );

  struct mg_str json = mg_str( jsonString );
  int len;
  int offset = mg_json_get( json, "$.deployment", &len );

  if ( offset < 0 )
  {
    LOG( PRINT_ERROR, "Invalid JSON: missing deployment" );
    return false;
  }

  struct mg_str deploymentObj = mg_str_n( json.buf + offset, len );

  offset = mg_json_get( deploymentObj, "$.update", &len );
  if ( offset >= 0 )
  {
    deployment->update = _get_action_type( mg_str_n( deploymentObj.buf + offset, len ).buf );
  }

  offset = mg_json_get( deploymentObj, "$.download", &len );
  if ( offset >= 0 )
  {
    deployment->download = _get_action_type( mg_str_n( deploymentObj.buf + offset, len ).buf );
  }

  deployment->chunkSize = 0;
  size_t chunkOffset = 0;
  while ( ( offset = mg_json_get( deploymentObj, "$.chunks", &len ) ) >= 0 && deployment->chunkSize < HAWKBIT_MAX_CHUNKS )
  {
    struct mg_str chunk = mg_str_n( deploymentObj.buf + offset + chunkOffset, len - chunkOffset );
    hawkbit_chunk_t* chunkPtr = &deployment->chunk[deployment->chunkSize++];

    offset = mg_json_get( chunk, "$.part", &len );
    if ( offset >= 0 )
    {
      snprintf( chunkPtr->part, sizeof( chunkPtr->part ), "%.*s", len, chunk.buf + offset );
    }

    offset = mg_json_get( chunk, "$.version", &len );
    if ( offset >= 0 )
    {
      snprintf( chunkPtr->version, sizeof( chunkPtr->version ), "%.*s", len, chunk.buf + offset );
    }

    offset = mg_json_get( chunk, "$.name", &len );
    if ( offset >= 0 )
    {
      snprintf( chunkPtr->name, sizeof( chunkPtr->name ), "%.*s", len, chunk.buf + offset );
    }

    chunkPtr->artifactsSize = 0;
    size_t artOffset = 0;
    while ( ( offset = mg_json_get( chunk, "$.artifacts", &len ) ) >= 0 && chunkPtr->artifactsSize < HAWKBIT_MAX_ARTIFACTS )
    {
      struct mg_str artifact = mg_str_n( chunk.buf + offset + artOffset, len - artOffset );
      hawkbit_artifacts_t* artifactPtr = &chunkPtr->artifacts[chunkPtr->artifactsSize++];

      offset = mg_json_get( artifact, "$.filename", &len );
      if ( offset >= 0 )
      {
        snprintf( artifactPtr->filename, sizeof( artifactPtr->filename ), "%.*s", len, artifact.buf + offset );
      }

      offset = mg_json_get( artifact, "$.size", &len );
      if ( offset >= 0 )
      {
        artifactPtr->size = (size_t) atoi( mg_str_n( artifact.buf + offset, len ).buf );
      }

      offset = mg_json_get( artifact, "$._links.download-http.href", &len );
      if ( offset >= 0 )
      {
        snprintf( artifactPtr->download_http, sizeof( artifactPtr->download_http ), "%.*s", len, artifact.buf + offset );
      }
    }
  }

  return true;
}

bool HAWKBITParser_ParseCancelAction( const char* jsonString, int* actionId )
{
  struct mg_str json = mg_str( jsonString );
  int len;
  int offset = mg_json_get( json, "$.cancelAction.stopId", &len );

  if ( offset >= 0 )
  {
    *actionId = atoi( mg_str_n( json.buf + offset, len ).buf );
    return true;
  }

  return false;
}
