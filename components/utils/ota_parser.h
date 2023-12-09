/**
 *******************************************************************************
 * @file    ota_parser.h
 * @author  Dmytro Shevchenko
 * @brief   OTA http messages parser header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/
#ifndef __OTA_PARSER_H__
#define __OTA_PARSER_H__

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Public macros -------------------------------------------------------------*/
#define OTA_URL_SIZE      256
#define OTA_MAX_ARTIFACTS 3
#define OTA_MAX_CHUNKS    3

/* Public types --------------------------------------------------------------*/

typedef enum
{
  OTA_DEPLOYMENT_ACTION_UNKNOWN,
  OTA_DEPLOYMENT_ACTION_FORCED,
  OTA_DEPLOYMENT_ACTION_SOFT,
  OTA_DEPLOYMENT_ACTION_DOWNLOAD_ONLY,
  OTA_DEPLOYMENT_ACTION_TIME_FORCED,
  OTA_DEPLOYMENT_ACTION_LAST
} ota_deployment_action_t;

typedef struct
{
  char filename[32];
  /* hashes not used */
  size_t size;
  char download_http[OTA_URL_SIZE];
  /* md5sum-http not used*/
} ota_artifacts_t;

typedef struct
{
  char part[32];
  char version[32];
  char name[32];
  size_t artifactsSize;
  ota_artifacts_t artifacts[OTA_MAX_CHUNKS];
} ota_chunk_t;

typedef struct
{
  ota_deployment_action_t download;
  ota_deployment_action_t update;
  size_t chunkSize;
  ota_chunk_t chunk[OTA_MAX_ARTIFACTS];
} ota_deployment_t;

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
bool OTAParser_ParseUrl( const char* jsonString, char* urlConfigData, size_t urlConfigDataSize, char* urlDeploymentBase, size_t urlDeploymentBaseSize );

/**
 * @brief   Parse deployments response json
 * @param   [in] jsonString - json response from hawkBit server.
 * @param   [out] deployment - response struture data.
 * @return  true if success
 */
bool OTAParse_ParseDeployment( const char* jsonString, ota_deployment_t* deployment );
#endif