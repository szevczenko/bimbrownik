#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#define T_WIFI_TYPE_SERVER 1
#define T_WIFI_TYPE_CLIENT 2

///////////////////// LOGS //////////////////////

#define CONFIG_DEBUG_WIFI            1
#define CONFIG_DEBUG_APP_EVENT       1
#define CONFIG_DEBUG_APP_MANAGER     1
#define CONFIG_DEBUG_NETWORK_MANAGER 1
#define CONFIG_DEBUG_TEMPERATURE     1
#define CONFIG_DEBUG_JSON            1
#define CONFIG_DEBUG_TCP_SERVER      1
#define CONFIG_DEBUG_TCP_TRANSPORT   1

/////////////////////  CONFIG PERIPHERALS  ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
// CONSOLE
#define CONFIG_CONSOLE_VSNPRINTF_BUFF_SIZE 64
#define CONFIG_USE_CONSOLE_TELNET          1
#define CONFIG_CONSOLE_SERIAL_SPEED        115200

///////////////////////////////////////////////////////////////////////////////////////////
//// LED

//////////////////////////////////////  END  //////////////////////////////////////////////

#define NORMALPRIOR 5

#ifndef BOARD_NAME
#define BOARD_NAME "ESP-WROOM-32"
#endif

#ifndef BOARD_VERSION
#define BOARD_VERSION \
  {                   \
    1, 0, 0           \
  }
#endif

#ifndef SOFTWARE_VERSION
#define SOFTWARE_VERSION \
  {                      \
    1, 0, 0              \
  }
#endif

#ifndef DEFAULT_DEV_TYPE
#define DEFAULT_DEV_TYPE 1
#endif

#define CONFIG_BUFF_SIZE 512
#define ESP_OK           0
#define TIME_IMMEDIATE   0
#define NORMALPRIO       5

#define MS2ST( ms )   ( ( ms ) / portTICK_RATE_MS )
#define ST2MS( tick ) ( (tick) *portTICK_RATE_MS )

#define osDelay( ms )       vTaskDelay( MS2ST( ms ) )
#define debug_printf( ... ) config_printf( __VA_ARGS__ )

enum config_print_lvl
{
  PRINT_DEBUG,
  PRINT_INFO,
  PRINT_WARNING,
  PRINT_ERROR,
  PRINT_TOP,
};

typedef struct
{
  const uint32_t start_config;
  uint32_t can_id;
  char name[32];
  uint8_t hw_ver[3];
  uint8_t sw_ver[3];
  uint8_t dev_type;
  uint8_t wifi_type;
  const uint32_t end_config;
} config_t;

typedef int esp_err_t;

extern config_t config;

int configSave( config_t* config );
int configRead( config_t* config );

void config_printf( enum config_print_lvl module_lvl, enum config_print_lvl msg_lvl, const char* format, ... );

void configInit( void );

#endif /* CONFIG_H_ */