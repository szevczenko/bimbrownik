/**
 *******************************************************************************
 * @file    hawkbit_parser.h
 * @author  Dmytro Shevchenko
 * @brief   HAWKBIT http messages parser header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/
#ifndef __HAWKBIT_PARSER_H__
#define __HAWKBIT_PARSER_H__

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Public macros -------------------------------------------------------------*/
#define HAWKBIT_URL_SIZE      256
#define HAWKBIT_MAX_ARTIFACTS 3
#define HAWKBIT_MAX_CHUNKS    3

/* Public types --------------------------------------------------------------*/

typedef enum
{
  HAWKBIT_DEPLOYMENT_ACTION_UNKNOWN,
  HAWKBIT_DEPLOYMENT_ACTION_FORCED,
  HAWKBIT_DEPLOYMENT_ACTION_SOFT,
  HAWKBIT_DEPLOYMENT_ACTION_DOWNLOAD_ONLY,
  HAWKBIT_DEPLOYMENT_ACTION_TIME_FORCED,
  HAWKBIT_DEPLOYMENT_ACTION_LAST
} hawkbit_deployment_action_t;

typedef struct
{
  char filename[32];
  /* hashes not used */
  size_t size;
  char download_http[HAWKBIT_URL_SIZE];
  /* md5sum-http not used*/
} hawkbit_artifacts_t;

typedef struct
{
  char part[32];
  char version[32];
  char name[32];
  size_t artifactsSize;
  hawkbit_artifacts_t artifacts[HAWKBIT_MAX_CHUNKS];
} hawkbit_chunk_t;

typedef struct
{
  hawkbit_deployment_action_t download;
  hawkbit_deployment_action_t update;
  size_t chunkSize;
  hawkbit_chunk_t chunk[HAWKBIT_MAX_ARTIFACTS];
} hawkbit_deployment_t;

/* Public functions --------------------------------------------------------------*/

/**
 * @brief   Gets urls from hawkBit json response 
 * @param   [in] jsonString - json response from hawkBit server.
 * @param   [out] urlConfigData - config data url for post device properties.
 * @param   [in] urlConfigDataSize - size of @c urlConfigData buffer.
 * @param   [out] urlDeploymentBase - url deployment base for get url artifacts.
 * @param   [in] urlDeploymentBaseSize - size of @c urlDeploymentBaseSize buffer.
 * @return  true if success
 */
bool HAWKBITParser_ParseUrl( const char* jsonString, char* urlConfigData, size_t urlConfigDataSize, char* urlDeploymentBase, size_t urlDeploymentBaseSize );

/**
 * @brief   Parse deployments response json
 * @param   [in] jsonString - json response from hawkBit server.
 * @param   [out] deployment - response struture data.
 * @return  true if success
 */
bool HAWKBITParse_ParseDeployment( const char* jsonString, hawkbit_deployment_t* deployment );
#endif