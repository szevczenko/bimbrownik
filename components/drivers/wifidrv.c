/**
 *******************************************************************************
 * @file    wifidrv.c
 * @author  Dmytro Shevchenko
 * @brief   Wifi Drv
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

#include "wifidrv.h"

#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "wifi.h"
#include "wifi_manager.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[WiFi] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_WIFI
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define CONFIG_TCPIP_EVENT_THD_WA_SIZE 2046

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                             \
  STATE( DISABLED, _wifi_disabled_state_handler_array ) \
  STATE( IDLE, _wifi_idle_state_handler_array )

/** @brief  Private types */
typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} wifi_drv_state_t;

typedef struct
{
  wifi_drv_state_t state;
  wifiType_t type;
  int retry;
  bool is_power_save;
  bool connected;
  bool disconnect_req;
  bool connect_req;
  bool read_wifi_data;
  uint32_t connect_attemps;
  uint32_t reason_disconnect;
  wifiConData_t wifi_con_data;
  int8_t rssi;
  QueueHandle_t queue;
} wifi_drv_ctx_t;

typedef enum
{
  //TIMER_ID_CONNECT_TIMEOUT,
  TIMER_ID_UPDATE_WIFI_INFO,
  TIMER_ID_LAST
} timer_id;

/* Private variables ---------------------------------------------------------*/

static wifi_drv_ctx_t ctx;

/* Private functions declaration ---------------------------------------------*/
static void _timeout_connect_cb( TimerHandle_t xTimer );
static void _update_wifi_info_cb( TimerHandle_t xTimer );

static void _state_common_event_deinit_request( const app_event_t* event );

static void _state_disabled_event_init_request( const app_event_t* event );

static void _state_idle_event_update_wifi_info( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _wifi_disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_event_init_request ),
};

static const struct app_events_handler _wifi_idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_WIFI_UPDATE_WIFI_INFO, _state_idle_event_update_wifi_info ),
    EVENT_ITEM( MSG_ID_DEINIT_REQ, _state_common_event_deinit_request ),
};

struct state_context
{
  const wifi_drv_state_t state;
  const char* name;
  const struct app_events_handler* event_handler_array;
  uint8_t event_handler_array_size;
};

static const struct state_context wifi_drv_state[STATE_TOP] =
  {
#define STATE( _state, _event_handler_array )    \
  {                                              \
    .state = _state,                             \
    .name = #_state,                             \
    .event_handler_array = _event_handler_array, \
    .event_handler_array_size = ARRAY_SIZE( _event_handler_array ) },
    STATE_HANDLER_ARRAY
#undef STATE
};

static app_timer_t timers[] =
  {
    TIMER_ITEM( TIMER_ID_UPDATE_WIFI_INFO, _update_wifi_info_cb, 1000, "WiFiUpdate" ) };

/* Private functions ---------------------------------------------------------*/

/** @brief  States functions. */
static const char* _get_state_name( const wifi_drv_state_t state )
{
  return wifi_drv_state[state].name;
}

static void _change_state( wifi_drv_state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_WIFI_DRV, APP_EVENT_WIFI_DRV );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_WIFI_DRV, APP_EVENT_WIFI_DRV, data, data_size );
  }

  WifiDrvPostMsg( &event );
}

static void _wifi_get_ip_address_cb(void * arg)
{
  app_event_t response = { 0 };
  wifi_drv_err_t err = WIFI_DRV_ERR_CONNECTED;
  AppEventPrepareWithData( &response, MSG_ID_NETWORK_MANAGER_WIFI_CONNECT_STATUS, APP_EVENT_WIFI_DRV, APP_EVENT_NETWORK_MANAGER, &err, sizeof( err ) );
  NetworkManagerPostMsg( &response );
}

static void _wifi_get_disconnected_cb(void * arg)
{
  app_event_t response = { 0 };
  wifi_drv_err_t err = WIFI_DRV_ERR_DISCONNECTED;
  AppEventPrepareWithData( &response, MSG_ID_NETWORK_MANAGER_WIFI_CONNECT_STATUS, APP_EVENT_WIFI_DRV, APP_EVENT_NETWORK_MANAGER, &err, sizeof( err ) );
  NetworkManagerPostMsg( &response );
}

static void _update_wifi_info_cb( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_WIFI_UPDATE_WIFI_INFO, NULL, 0 );
}

/* Sate machine functions ---------------------------------------------------*/

static void _state_disabled_event_init_request( const app_event_t* event )
{
  wifi_manager_start();
  wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &_wifi_get_ip_address_cb);
  wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &_wifi_get_disconnected_cb);

  wifi_drv_err_t err = WIFI_DRV_ERR_OK;
  app_event_t response = { 0 };
  AppEventPrepareWithData( &response, MSG_ID_INIT_RES, APP_EVENT_WIFI_DRV, APP_EVENT_NETWORK_MANAGER, &err, sizeof( err ) );
  NetworkManagerPostMsg( &response );
  _change_state( IDLE );
}

static void _state_idle_event_update_wifi_info( const app_event_t* event )
{
  WiFiReadInfo( &ctx.rssi );
  AppTimerStart( timers, TIMER_ID_UPDATE_WIFI_INFO );
}

static void _state_common_event_deinit_request( const app_event_t* event )
{
  assert( WiFiDeinit() == WIFI_ERR_OK );
  _change_state( DISABLED );
}

static void wifi_event_task( void* pv )
{
  while ( 1 )
  {
    app_event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      AppEventSearchAndExecute( &event, wifi_drv_state[ctx.state].event_handler_array, wifi_drv_state[ctx.state].event_handler_array_size );
      AppEventDelete( &event );
    }
  }
}

/* Public functions -----------------------------------------------------------*/

void WifiDrvPostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void wifiDrvInit( wifiType_t type )
{
  assert( type < WIFI_TYPE_LAST );
  ctx.type = type;
  ctx.queue = xQueueCreate( 8, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( wifi_event_task, "wifi_event_task", CONFIG_TCPIP_EVENT_THD_WA_SIZE, NULL, NORMALPRIOR, NULL );
}
