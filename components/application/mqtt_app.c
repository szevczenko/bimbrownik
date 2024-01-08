/**
 *******************************************************************************
 * @file    mqtt_app.c
 * @author  Dmytro Shevchenko
 * @brief   MQTT application layer
 *******************************************************************************
 */
#include "mqtt_app.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_events.h"
#include "app_timers.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include "mqtt_config.h"
#include "mqtt_json_parser.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[MQTT App] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_MQTT_APP
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private types -------------------------------------------------------------*/

/** @brief  Array with defined states */
#define STATE_HANDLER_ARRAY                        \
  STATE( DISABLED, _disabled_state_handler_array ) \
  STATE( IDLE, _idle_state_handler_array )         \
  STATE( CONNECT, _connect_state_handler_array )   \
  STATE( WORK, _work_state_handler_array )

typedef enum
{
#define STATE( _state, _event_handler_array ) _state,
  STATE_HANDLER_ARRAY
#undef STATE
    STATE_TOP,
} module_state_t;

typedef struct
{
  module_state_t state;
  QueueHandle_t queue;
  esp_mqtt_client_handle_t client;

  bool is_eth_connected;
  bool is_mqtt_connected;

} module_ctx_t;

typedef enum
{
  TIMER_ID_TRY_RECONNECT,
  TIMER_ID_TIMEOUT_CONNECT,
  TIMER_ID_LAST
} timer_id;

/* Private functions declaration ---------------------------------------------*/
static void _timer_try_reconnect( TimerHandle_t xTimer );
static void _timer_timeout_connect( TimerHandle_t xTimer );

static void _state_common_eth_connect( const app_event_t* event );
static void _state_common_eth_disconnect( const app_event_t* event );
static void _state_common_mqtt_disconnect( const app_event_t* event );

static void _state_disabled_init( const app_event_t* event );

static void _state_idle_event_connect( const app_event_t* event );
// static void _state_idle_event_update_config( const app_event_t* event );

static void _state_connect_event_connect( const app_event_t* event );
static void _state_connect_event_update_config( const app_event_t* event );
// static void _state_connect_event_subscribe( const app_event_t* event );

static void _state_work_event_update_config( const app_event_t* event );
static void _state_work_event_post_data( const app_event_t* event );

/* Status callbacks declaration. ---------------------------------------------*/
static const struct app_events_handler _disabled_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_INIT_REQ, _state_disabled_init ),
};

static const struct app_events_handler _idle_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_MQTT_ETH_CONNECTED, _state_common_eth_connect ),
    EVENT_ITEM( MSG_ID_MQTT_ETH_DISCONNECTED, _state_common_eth_disconnect ),
    // EVENT_ITEM( MSG_ID_MQTT_APP_UPDATE_CONFIG, _state_idle_event_update_config ),
    EVENT_ITEM( MSG_ID_MQTT_APP_CONNECT, _state_idle_event_connect ),
    EVENT_ITEM( MSG_ID_MQTT_APP_DISCONNECT, _state_common_mqtt_disconnect ),
};

static const struct app_events_handler _connect_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_MQTT_APP_CONNECT, _state_connect_event_connect ),
    EVENT_ITEM( MSG_ID_MQTT_ETH_DISCONNECTED, _state_common_eth_disconnect ),
    EVENT_ITEM( MSG_ID_MQTT_APP_UPDATE_CONFIG, _state_connect_event_update_config ),
    EVENT_ITEM( MSG_ID_MQTT_APP_DISCONNECT, _state_common_mqtt_disconnect ),
    // EVENT_ITEM( MSG_ID_MQTT_APP_SUBSCRIBE, _state_connect_event_subscribe ),
};

static const struct app_events_handler _work_state_handler_array[] =
  {
    EVENT_ITEM( MSG_ID_MQTT_ETH_CONNECTED, _state_common_eth_connect ),
    EVENT_ITEM( MSG_ID_MQTT_ETH_DISCONNECTED, _state_common_eth_disconnect ),
    EVENT_ITEM( MSG_ID_MQTT_APP_UPDATE_CONFIG, _state_work_event_update_config ),
    EVENT_ITEM( MSG_ID_MQTT_APP_POST_DATA, _state_work_event_post_data ),
    EVENT_ITEM( MSG_ID_MQTT_APP_DISCONNECT, _state_common_mqtt_disconnect ),
};

/* Private variables ---------------------------------------------------------*/

static module_ctx_t ctx;

struct state_context
{
  const module_state_t state;
  const char* name;
  const struct app_events_handler* event_handler_array;
  uint8_t event_handler_array_size;
};

static const struct state_context module_state[STATE_TOP] =
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
    TIMER_ITEM( TIMER_ID_TRY_RECONNECT, _timer_try_reconnect, 30000, "MqttReconnect" ),
    TIMER_ITEM( TIMER_ID_TIMEOUT_CONNECT, _timer_timeout_connect, 10000, "MqttTimeoutConn" ),
};

/* Private functions ---------------------------------------------------------*/

static const char* _get_state_name( const module_state_t state )
{
  return module_state[state].name;
}

static void _change_state( module_state_t new_state )
{
  LOG( PRINT_INFO, "State: %s -> %s", _get_state_name( ctx.state ), _get_state_name( new_state ) );
  ctx.state = new_state;
}

static void _send_internal_event( app_msg_id_t id, const void* data, uint32_t data_size )
{
  app_event_t event = {};
  if ( data_size == 0 )
  {
    AppEventPrepareNoData( &event, id, APP_EVENT_MQTT_APP, APP_EVENT_MQTT_APP );
  }
  else
  {
    AppEventPrepareWithData( &event, id, APP_EVENT_MQTT_APP, APP_EVENT_MQTT_APP, data, data_size );
  }

  MQTTApp_PostMsg( &event );
}

static bool _subscribe( void )
{
  char* config_topic = (char*) MQTTConfig_GetString( MQTT_CONFIG_VALUE_TOPIC_PREFIX );
  char subscribe_topic[MQTT_CONFIG_STR_SIZE + 4];
  // uint32_t sn = DevConfig_GetSerialNumber();
  sprintf( subscribe_topic, "%s/#", config_topic );
  // char* control_topic = (char*) MQTTConfig_GetString( MQTT_CONFIG_VALUE_CONTROL_TOPIC );
  int msg_id = esp_mqtt_client_subscribe( ctx.client, subscribe_topic, 0 );
  if ( msg_id == -1 )
  {
    LOG( PRINT_ERROR, "Failed subscribe, msg_id=%d", msg_id );
    return false;
  }
  return true;
}

static void log_error_if_nonzero( const char* message, int error_code )
{
  if ( error_code != 0 )
  {
    LOG( PRINT_WARNING, "Last error %s: 0x%x", message, error_code );
  }
}

static void _mqtt_event_handler( void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data )
{
  LOG( PRINT_DEBUG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id );
  esp_mqtt_event_handle_t event = event_data;

  switch ( (esp_mqtt_event_id_t) event_id )
  {
    case MQTT_EVENT_CONNECTED:
      if ( false == _subscribe() )
      {
        _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
      }
      break;
    case MQTT_EVENT_DISCONNECTED:
      _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
      break;

    case MQTT_EVENT_SUBSCRIBED:
      ctx.is_mqtt_connected = true;
      AppTimerStop( timers, TIMER_ID_TIMEOUT_CONNECT );
      _change_state( WORK );
      _send_internal_event( MSG_ID_MQTT_APP_POST_DATA, NULL, 0 );
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      // if ( false == _subscribe() )
      // {
      //   _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
      // }
      LOG( PRINT_WARNING, "MQTT_EVENT_UNSUBSCRIBED ToDo test this event" );
      break;
    case MQTT_EVENT_PUBLISHED:
      LOG( PRINT_INFO, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id );
      break;
    case MQTT_EVENT_DATA:
      LOG( PRINT_INFO, "MQTT_EVENT_DATA" );
      printf( "TOPIC=%.*s\r\n", event->topic_len, event->topic );
      printf( "DATA=%.*s\r\n", event->data_len, event->data );
      char* prefix = (char*) MQTTConfig_GetString( MQTT_CONFIG_VALUE_TOPIC_PREFIX );
      if ( memcmp( prefix, event->topic, strlen( prefix ) ) == 0 )
      {
        size_t offset = strlen( prefix ) + 1;
        MQTTJsonParse( &event->topic[offset], event->topic_len - offset, event->data, event->data_len, NULL, 0 );
      }
      break;
    case MQTT_EVENT_ERROR:
      LOG( PRINT_INFO, "MQTT_EVENT_ERROR" );
      if ( event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT )
      {
        log_error_if_nonzero( "reported from esp-tls", event->error_handle->esp_tls_last_esp_err );
        log_error_if_nonzero( "reported from tls stack", event->error_handle->esp_tls_stack_err );
        log_error_if_nonzero( "captured as transport's socket errno", event->error_handle->esp_transport_sock_errno );
        LOG( PRINT_INFO, "Last errno string (%s)", strerror( event->error_handle->esp_transport_sock_errno ) );
        _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
      }
      break;
    default:
      LOG( PRINT_INFO, "Other event id:%d", event->event_id );
      break;
  }
}

static void _timer_try_reconnect( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_MQTT_APP_CONNECT, NULL, 0 );
}

static void _timer_timeout_connect( TimerHandle_t xTimer )
{
  _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
}

static void _state_common_eth_connect( const app_event_t* event )
{
  ctx.is_eth_connected = true;
  _send_internal_event( MSG_ID_MQTT_APP_CONNECT, NULL, 0 );
}

static void _state_common_eth_disconnect( const app_event_t* event )
{
  ctx.is_eth_connected = false;
  _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
}

static void _state_common_mqtt_disconnect( const app_event_t* event )
{
  if ( ctx.client != NULL )
  {
    assert( ESP_OK == esp_mqtt_client_destroy( ctx.client ) );
    ctx.client = NULL;
  }

  if ( ctx.is_eth_connected )
  {
    AppTimerStart( timers, TIMER_ID_TRY_RECONNECT );
  }
  ctx.is_mqtt_connected = false;
  _change_state( IDLE );
}

static void _state_disabled_init( const app_event_t* event )
{
  _change_state( IDLE );

  if ( ctx.is_eth_connected )
  {
    _send_internal_event( MSG_ID_MQTT_APP_CONNECT, NULL, 0 );
  }
}

static void _state_idle_event_connect( const app_event_t* event )
{
  assert( false == ctx.is_mqtt_connected );
  _change_state( CONNECT );
  _send_internal_event( MSG_ID_MQTT_APP_CONNECT, NULL, 0 );
  AppTimerStop( timers, TIMER_ID_TRY_RECONNECT );
}

static void _state_connect_event_connect( const app_event_t* event )
{
  bool use_ssl = false;
  const char* address = MQTTConfig_GetString( MQTT_CONFIG_VALUE_ADDRESS );
  const char* username = MQTTConfig_GetString( MQTT_CONFIG_VALUE_USERNAME );
  const char* password = MQTTConfig_GetString( MQTT_CONFIG_VALUE_PASSWORD );
  const char* cert = MQTTConfig_GetCert( MQTT_CONFIG_VALUE_CERT );
  MQTTConfig_GetBool( &use_ssl, MQTT_CONFIG_VALUE_SSL );
  esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = address,
  };

  if ( use_ssl )
  {
    mqtt_cfg.broker.verification.certificate = cert;
  }

  if ( strlen( username ) != 0 )
  {
    mqtt_cfg.credentials.username = username;
  }

  if ( strlen( password ) != 0 )
  {
    mqtt_cfg.credentials.authentication.password = password;
  }

  if ( strlen( cert ) != 0 )
  {
    mqtt_cfg.broker.verification.certificate = cert;
  }

  ctx.client = esp_mqtt_client_init( &mqtt_cfg );
  /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
  esp_mqtt_client_register_event( ctx.client, ESP_EVENT_ANY_ID, _mqtt_event_handler, NULL );
  esp_mqtt_client_start( ctx.client );
  AppTimerStart( timers, TIMER_ID_TIMEOUT_CONNECT );
}

static void _state_connect_event_update_config( const app_event_t* event )
{
  _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
}

// static void _state_connect_event_subscribe( const app_event_t* event )
// {

// }

static void _state_work_event_update_config( const app_event_t* event )
{
  _send_internal_event( MSG_ID_MQTT_APP_DISCONNECT, NULL, 0 );
}

static void _state_work_event_post_data( const app_event_t* event )
{
  const char* data_topic = MQTTConfig_GetString( MQTT_CONFIG_VALUE_POST_DATA_TOPIC );
  const char* test_data = "{\"temp\":123}";
  int msg_id = esp_mqtt_client_publish( ctx.client, data_topic, test_data, 0, 0, 0 );
  if ( msg_id < 0 )
  {
    LOG( PRINT_WARNING, "publish data fail" );
  }
}

static void _task( void* pv )
{
  _send_internal_event( MSG_ID_INIT_REQ, NULL, 0 );
  while ( 1 )
  {
    app_event_t event = { 0 };
    if ( xQueueReceive( ctx.queue, &( event ), portMAX_DELAY ) == pdPASS )
    {
      AppEventSearchAndExecute( &event, module_state[ctx.state].event_handler_array, module_state[ctx.state].event_handler_array_size );
      AppEventDelete( &event );
    }
  }
}

static void _update_config_cb( void )
{
  _send_internal_event( MSG_ID_MQTT_APP_UPDATE_CONFIG, NULL, 0 );
}

/* Public functions -----------------------------------------------------------*/

void MQTTApp_PostMsg( app_event_t* event )
{
  if ( xQueueSend( ctx.queue, (void*) event, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void MQTTApp_Init( void )
{
  MQTTConfig_Init();
  MQTTConfig_SetCallback( _update_config_cb );
  ctx.queue = xQueueCreate( 8, sizeof( app_event_t ) );
  assert( ctx.queue );
  AppTimersInit( timers, TIMER_ID_LAST );
  xTaskCreate( _task, "mqtt_app", 3072, NULL, NORMALPRIOR, NULL );
}

bool MqttApp_PostData( const char* topic, const char* msg )
{
  assert( topic );
  assert( msg );
  if ( ctx.state != WORK )
  {
    return false;
  }
  char post_topic[128] = {};
  const char* prefix = MQTTConfig_GetString( MQTT_CONFIG_VALUE_POST_DATA_TOPIC );

  snprintf( post_topic, sizeof( post_topic ) - 1, "%s/%s", prefix, topic );

  int msg_id = esp_mqtt_client_publish( ctx.client, post_topic, msg, 0, 0, 0 );
  if ( msg_id < 0 )
  {
    LOG( PRINT_WARNING, "publish data fail" );
    return false;
  }
  return true;
}