#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dev_config.h"

///////////////////// LOGS //////////////////////

#define CONFIG_DEBUG_WIFI            1
#define CONFIG_DEBUG_APP_EVENT       1
#define CONFIG_DEBUG_APP_MANAGER     1
#define CONFIG_DEBUG_NETWORK_MANAGER 1
#define CONFIG_DEBUG_TEMPERATURE     1
#define CONFIG_DEBUG_JSON            1
#define CONFIG_DEBUG_TCP_SERVER      1
#define CONFIG_DEBUG_TCP_TRANSPORT   1
#define CONFIG_DEBUG_MDNS            1
#define CONFIG_DEBUG_MQTT_APP        1
#define CONFIG_DEBUG_DEVICE_MANAGER  1
#define CONFIG_DEBUG_HTTP_SERVER     1
#define CONFIG_DEBUG_HTTP_HAWKBIT    1

//////////////  CONFIG MODULES  //////////////////
#define NORMALPRIOR 5

#define CONFIG_BUFF_SIZE 512
#define ESP_OK           0
#define NORMALPRIO       5

#define MS2ST( ms )   ms
#define ST2MS( tick ) tick

#define osDelay( ms )       sleep( ms )
#define debug_printf( ... ) DevConfig_Printf( __VA_ARGS__ )
#endif /* CONFIG_H_ */
