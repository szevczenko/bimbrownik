/**
 *******************************************************************************
 * @file    app_msg_id.h
 * @author  Dmytro Shevchenko
 * @brief   Application messages id
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/
#ifndef __MSG_IDS_H__
#define __MSG_IDS_H__

#define MSG_IDS_LIST                       \
  MSG( INIT_REQ )                          \
  MSG( INIT_RES )                          \
  MSG( DEINIT_REQ )                        \
                                           \
  /* App manager ids*/                     \
  MSG( APP_MANAGER_INIT_REQ )              \
  MSG( APP_MANAGER_INIT_RES )              \
  MSG( APP_MANAGER_TIMEOUT_INIT )          \
  MSG( APP_MANAGER_TEMP_SENSORS_SCAN_RES ) \
                                           \
  /* Network manager ids */                \
  MSG( NETWORK_MANAGER_TIMEOUT_INIT )      \
  MSG( NETWORK_MANAGER_INIT_RES )          \
  MSG( NETWORK_MANAGER_WIFI_CONNECT_REQ )  \
  MSG( NETWORK_MANAGER_WIFI_CONNECT_RES )  \
                                           \
  /* Wifi msg ids */                       \
  MSG( WIFI_CONNECT_REQ )                  \
  MSG( WIFI_CONNECT_RES )                  \
  MSG( WIFI_DISCONNECT_REQ )               \
  MSG( WIFI_DISCONNECT_RES )               \
  MSG( WIFI_WPS_REQ )                      \
  MSG( WIFI_WPS_RES )                      \
  MSG( WIFI_WPS_STOP )                     \
  MSG( WIFI_SCAN_REQ )                     \
  MSG( WIFI_SCAN_RES )                     \
                                           \
  /* Wifi internal msg ids */              \
  MSG( WIFI_SCAN_STOP )                    \
  MSG( WIFI_TIMEOUT_CONNECT )              \
  MSG( WIFI_UPDATE_WIFI_INFO )             \
                                           \
  /* Temperature msg ids */                \
  MSG( TEMPERATURE_SCAN_DEVICES_REQ )      \
  MSG( TEMPERATURE_SCAN_DEVICES_RES )      \
  MSG( TEMPERATURE_START_MEASURE )         \
  MSG( TEMPERATURE_STOP_MEASURE )          \
                                           \
  /* Temperature internal msg ids */       \
  MSG( TEMPERATURE_MEASURE_REQ )

/* Public types -------------------------------------------------------------*/

typedef enum
{
#define MSG( _enum_id ) MSG_ID_##_enum_id,
  MSG_IDS_LIST
#undef MSG
} app_msg_id_t;

#endif /* __MSG_IDS_H__ */
