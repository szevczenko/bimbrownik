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

#define MSG_IDS_LIST                              \
  MSG( INIT_REQ )                                 \
  MSG( INIT_RES )                                 \
  MSG( DEINIT_REQ )                               \
                                                  \
  /* App manager ids*/                            \
  MSG( APP_MANAGER_INIT_REQ )                     \
  MSG( APP_MANAGER_INIT_RES )                     \
  MSG( APP_MANAGER_TIMEOUT_INIT )                 \
  MSG( APP_MANAGER_TEMP_SENSORS_SCAN_RES )        \
  MSG( APP_MANAGER_TEMP_WPS_TEST )                \
                                                  \
  /* Network manager ids */                       \
  MSG( NETWORK_MANAGER_TIMEOUT_INIT )             \
  MSG( NETWORK_MANAGER_INIT_RES )                 \
  MSG( NETWORK_MANAGER_TCP_SERVER_CLIENT_STATUS ) \
  MSG( NETWORK_MANAGER_WIFI_CONNECT_STATUS )      \
                                                  \
  /* Wifi internal msg ids */                     \
  MSG( WIFI_UPDATE_WIFI_INFO )                    \
                                                  \
  /* Temperature msg ids */                       \
  MSG( TEMPERATURE_SCAN_DEVICES_REQ )             \
  MSG( TEMPERATURE_SCAN_DEVICES_RES )             \
  MSG( TEMPERATURE_START_MEASURE )                \
  MSG( TEMPERATURE_STOP_MEASURE )                 \
                                                  \
  /* Temperature internal msg ids */              \
  MSG( TEMPERATURE_MEASURE_REQ )                  \
                                                  \
  /* TCP Server msg ids */                        \
  MSG( TCP_SERVER_ETHERNET_CONNECTED )            \
  MSG( TCP_SERVER_ETHERNET_DISCONNECTED )         \
  MSG( TCP_SERVER_SEND_DATA )                     \
                                                  \
  /* OTA */                                       \
  MSG( OTA_POLL_SERVER )                          \
  MSG( OTA_POST_CONFIG_DATA )                     \
  MSG( OTA_DOWNLOAD_IMAGE )                       \
  MSG( OTA_POST_OTA_RESULT )                      \
  MSG( OTA_STOP_POLL_SERVER )                     \
                                                  \
  /* MQTT */                                      \
  MSG( MQTT_APP_CONNECT )                         \
  MSG( MQTT_APP_SUBSCRIBE )                       \
  MSG( MQTT_ETH_CONNECTED )                       \
  MSG( MQTT_ETH_DISCONNECTED )                    \
  MSG( MQTT_APP_UPDATE_CONFIG )                   \
  MSG( MQTT_APP_POST_DATA )                       \
  MSG( MQTT_APP_DISCONNECT )                      \
                                                  \
  /* TCP Server internal msg ids */               \
  MSG( TCP_SERVER_WAIT_CONNECTION )               \
  MSG( TCP_SERVER_WAIT_CLIENT_DATA )              \
  MSG( TCP_SERVER_PREPARE_SOCKET )                \
  MSG( TCP_SERVER_CLOSE_SOCKET )

/* Public types -------------------------------------------------------------*/

typedef enum
{
#define MSG( _enum_id ) MSG_ID_##_enum_id,
  MSG_IDS_LIST
#undef MSG
} app_msg_id_t;

#endif /* __MSG_IDS_H__ */
