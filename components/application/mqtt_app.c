#include "mqtt_app.h"

#include "dev_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "mongoose.h"
#include "mongoose_task.h"
#include "mqtt_config.h"
#include "mqtt_json_parser.h"

#define RETRY_COUNT 3

// ...existing code...

typedef struct
{
  char topic[64];
  char message[128];
  int qos;
} mqtt_message_t;

static int initialized = 0;
static int connected = 0;
static int subscribed = 0;
static struct mg_connection* nc;
static TimerHandle_t reconnect_timer;
static TimerHandle_t puback_timer;
// static TimerHandle_t pubrec_timer;
static TimerHandle_t suback_timer;
static int puback_received = 0;
// static int pubrec_received = 0;
// static int pubcomp_received = 0;
static int retries = 0;
static struct mg_mqtt_opts opts;

static QueueHandle_t mqtt_queue;
static SemaphoreHandle_t puback_semaphore;

static void ev_handler( struct mg_connection* nc, int ev, void* ev_data );

static void _connect( void )
{
  if ( nc != NULL )
  {
    mg_mqtt_disconnect( nc, NULL );
    nc->is_closing = 1;
  }
  const char* address = MQTTConfig_GetString( MQTT_CONFIG_VALUE_ADDRESS );
  const char* username = MQTTConfig_GetString( MQTT_CONFIG_VALUE_USERNAME );
  const char* password = MQTTConfig_GetString( MQTT_CONFIG_VALUE_PASSWORD );
  const char* serial_number = DevConfig_GetSerialNumber();
  struct mg_mqtt_opts opts_con = { .user = mg_str( username ),
                                   .pass = mg_str( password ),
                                   .client_id = mg_str( serial_number ),
                                   .keepalive = 60,
                                   .clean = true };
  nc = mg_mqtt_connect( &mgr, address, &opts_con, ev_handler, NULL );
  assert( nc != NULL );
}

// Function to attempt reconnection
static void reconnect( TimerHandle_t xTimer )
{
  if ( !connected )
  {
    printf( "Attempting to reconnect ...\n" );
    _connect();
    assert( nc != NULL );
  }
}

// Function to retry sending QoS 1 message
static void retry_qos1_message( TimerHandle_t xTimer )
{
  if ( !puback_received && retries < RETRY_COUNT )
  {
    printf( "Retrying QoS 1 message, attempt %d...\n", retries + 1 );
    mg_mqtt_pub( nc, &opts );
    retries++;
  }
  else if ( retries >= RETRY_COUNT )
  {
    printf( "Failed to receive PUBACK after %d retries\n", RETRY_COUNT );
    xSemaphoreGive( puback_semaphore );
    xTimerStop( puback_timer, 0 );
  }
}

// Function to retry sending QoS 2 message
// static void retry_qos2_message( TimerHandle_t xTimer )
// {
//   if ( !pubrec_received || !pubcomp_received )
//   {
//     printf( "Retrying QoS 2 message...\n" );
//     mg_mqtt_pub( nc, &( struct mg_mqtt_opts ){ .topic = mg_str( pvTimerGetTimerID( xTimer ) ), .message = mg_str( timer->user_data ), .qos = 2 } );
//   }
// }

static void _subscribe( void )
{
  // Subscribe to topics
  const char* config_topic = MQTTConfig_GetString( MQTT_CONFIG_VALUE_TOPIC_PREFIX );
  char subscribe_topic[MQTT_CONFIG_STR_SIZE + 4] = { 0 };
  // uint32_t sn = DevConfig_GetSerialNumber();
  sprintf( subscribe_topic, "%s/#", config_topic );
  // char* control_topic = (char*) MQTTConfig_GetString( MQTT_CONFIG_VALUE_CONTROL_TOPIC );
  mg_mqtt_sub( nc, &( struct mg_mqtt_opts ){ .topic = mg_str( subscribe_topic ), .qos = 0 } );
  xTimerStart( suback_timer, 0 );
}

// Function to handle SUBACK timeout
static void suback_timeout( TimerHandle_t xTimer )
{
  if ( !subscribed )
  {
    _subscribe();
  }
}

static void _connected( void )
{
  connected = 1;
  const char* address = MQTTConfig_GetString( MQTT_CONFIG_VALUE_ADDRESS );
  if ( mg_url_is_ssl( address ) )
  {
    const char* cert = MQTTConfig_GetCert( MQTT_CONFIG_VALUE_CERT );

    struct mg_tls_opts opts_ca = {
      .ca = mg_str( cert ),
      .name = mg_url_host( address ),
    };
    mg_tls_init( nc, &opts_ca );
  }

  xTimerStop( reconnect_timer, 0 );
  printf( "Connected to server\n" );
  _subscribe();
}

// MQTT event handler
static void ev_handler( struct mg_connection* nc, int ev, void* ev_data )
{
  switch ( ev )
  {
    case MG_EV_CONNECT:
      _connected();
      break;
    case MG_EV_MQTT_CMD:
      {
        struct mg_mqtt_message* mm = (struct mg_mqtt_message*) ev_data;
        switch ( mm->cmd )
        {
          case MQTT_CMD_SUBACK:
            subscribed = 1;
            xTimerStop( suback_timer, 0 );
            printf( "Subscribed to topic\n" );
            break;
          case MQTT_CMD_UNSUBACK:
            printf( "Received UNSUBACK\n" );
            break;
          case MQTT_CMD_PINGRESP:
            printf( "Received PINGRESP\n" );
            break;
          case MQTT_CMD_PUBACK:
            puback_received = 1;
            xTimerStop( puback_timer, 0 );
            xSemaphoreGive( puback_semaphore );
            printf( "PUBACK received\n" );
            break;
          case MQTT_CMD_PUBREC:
            // pubrec_received = 1;
            // xTimerStop( pubrec_timer, 0 );
            // xTimerStart( pubrec_timer, 0 );
            printf( "PUBREC received\n" );
            break;
          case MQTT_CMD_PUBCOMP:
            // pubcomp_received = 1;
            // xTimerStop( pubrec_timer, 0 );
            // printf( "PUBCOMP received\n" );
            break;
          default:
            break;
        }
      }
      break;

    case MG_EV_MQTT_MSG:
      {
        struct mg_mqtt_message* mm = (struct mg_mqtt_message*) ev_data;
        MG_INFO( ( "%lu RECEIVED %.*s <- %.*s", nc->id, (int) mm->data.len,
                   mm->data.ptr, (int) mm->topic.len, mm->topic.ptr ) );

        char* prefix = (char*) MQTTConfig_GetString( MQTT_CONFIG_VALUE_TOPIC_PREFIX );
        if ( memcmp( prefix, mm->topic.ptr, strlen( prefix ) ) == 0 )
        {
          size_t offset = strlen( prefix ) + 1;
          MQTTJsonParse( &mm->topic.ptr[offset], mm->topic.len - offset, mm->data.ptr, mm->data.len, NULL, 0 );
        }
      }
      break;

    case MG_EV_CLOSE:
      connected = 0;
      subscribed = 0;
      printf( "Disconnected from server\n" );
      xTimerStart( reconnect_timer, 0 );
      break;
      // ...handle other events...
  }
}

static void _post( const char* topic, const char* message, int qos )
{
  puback_received = 0;
  // pubrec_received = 0;
  // pubcomp_received = 0;
  memset( &opts, 0, sizeof( opts ) );
  opts.qos = qos;
  opts.topic = mg_str( topic );
  opts.version = 4;
  opts.message = mg_str( message );
  retries = 0;
  mg_mqtt_pub( nc, &opts );

  if ( qos == 1 )
  {
    xTimerStart( puback_timer, 0 );
    if ( xSemaphoreTake( puback_semaphore, pdMS_TO_TICKS( 5000 * RETRY_COUNT + 100 ) ) == pdFALSE )
    {
      printf( "Failed to receive PUBACK after %d retries\n", RETRY_COUNT );
    }
  }
  // QoS 2 not supported
  // else if ( qos == 2 )
  // {
  //   xTimerStart( pubrec_timer, 0 );
  // }
}

// FreeRTOS task to poll the Mongoose driver
static void mongoose_task( void* pvParameters )
{
  mqtt_message_t msg;
  while ( 1 )
  {
    if ( xQueueReceive( mqtt_queue, &msg, portMAX_DELAY ) )
    {
      if ( connected )
      {
        _post( msg.topic, msg.message, msg.qos );
      }
    }
  }
}

static void _update_config_cb( void )
{
  MqttApp_Deinit();
  MqttApp_Init();
}

// Initialize the MQTT driver
void MqttApp_Init( void )
{
  assert( initialized == 0 );

  MQTTConfig_Init();
  MQTTConfig_SetCallback( _update_config_cb );

  puback_timer = xTimerCreate( "PubackTimer", pdMS_TO_TICKS( 5000 ), pdTRUE, NULL, retry_qos1_message );
  // pubrec_timer = xTimerCreate( "PubrecTimer", pdMS_TO_TICKS( 5000 ), pdTRUE, (void*) topic, retry_qos2_message );
  reconnect_timer = xTimerCreate( "ReconnectTimer", pdMS_TO_TICKS( 30000 ), pdTRUE, NULL, reconnect );
  suback_timer = xTimerCreate( "SubackTimer", pdMS_TO_TICKS( 30000 ), pdTRUE, NULL, suback_timeout );

  assert( puback_timer != NULL );
  // assert( pubrec_timer != NULL );
  assert( reconnect_timer != NULL );
  assert( suback_timer != NULL );

  mqtt_queue = xQueueCreate( 6, sizeof( mqtt_message_t ) );
  assert( mqtt_queue != NULL );

  puback_semaphore = xSemaphoreCreateBinary();
  assert( puback_semaphore != NULL );

  xTaskCreate( mongoose_task, "MongooseTask", 4096, NULL, 5, NULL );
  xTimerStart( reconnect_timer, 0 );
  _connect();
  initialized = 1;
}

// Deinitialize the MQTT driver
void MqttApp_Deinit( void )
{
  xTimerDelete( reconnect_timer, 0 );
  xTimerDelete( suback_timer, 0 );
  xTimerDelete( puback_timer, 0 );
  // xTimerDelete( pubrec_timer, 0 );

  vQueueDelete( mqtt_queue );
  vSemaphoreDelete( puback_semaphore );

  if ( connected )
  {
    mg_mqtt_disconnect( nc, NULL );
    nc->is_closing = 1;
    nc = NULL;
  }
  initialized = 0;
}

// Post a message to the MQTT server
bool MqttApp_PostData( const char* topic, const char* message, int qos )
{
  if ( initialized == 0 )
  {
    return false;
  }
  mqtt_message_t msg;
  strncpy( msg.topic, topic, sizeof( msg.topic ) );
  strncpy( msg.message, message, sizeof( msg.message ) );
  msg.qos = qos;
  return xQueueSend( mqtt_queue, &msg, 0 ) == pdTRUE;
}

// ...existing code...
