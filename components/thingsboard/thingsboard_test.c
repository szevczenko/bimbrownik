#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <string.h>

#include "mqtt_config.h"
#include "rpc_callback.h"
#include "shared_attribute_callback.h"
#include "thingsboard.h"

static const char* TAG = "TB_TEST";

// ThingsBoard Configuration
#define TB_TOKEN  "hwa0905vne270x1kltgv"
#define TB_SERVER "192.168.1.2"
#define TB_PORT   1883

// Test Configuration
#define TELEMETRY_SEND_INTERVAL_MS 30000
#define ATTRIBUTE_SEND_INTERVAL_MS 60000

// Global variables
static thingsboard_t tb_client;
static bool tb_connected = false;
static bool subscriptions_active = false;

// Test data
static float temperature = 25.0;
static float humidity = 60.0;
static bool led_state = false;
static int counter = 0;

// RPC method names
#define RPC_SET_LED_METHOD         "setLED"
#define RPC_GET_STATUS_METHOD      "getStatus"
#define RPC_SET_TEMPERATURE_METHOD "setTemperature"

// Shared attribute keys
#define SHARED_ATTR_FIRMWARE_VERSION "fw_version"
#define SHARED_ATTR_CONFIG_INTERVAL  "config_interval"
#define SHARED_ATTR_DEBUG_MODE       "debug_mode"

// RPC Callback Functions

/**
 * @brief   RPC callback for setLED method.
 */
static size_t rpc_set_led_callback( const char* request_data, size_t request_len,
                                    char* response_data, size_t response_max_len )
{
  ESP_LOGI( TAG, "RPC setLED called with data: %.*s", (int) request_len, request_data );

  // Parse the LED state from JSON (simplified parsing)
  bool new_led_state = ( strstr( request_data, "true" ) != NULL );
  led_state = new_led_state;

  ESP_LOGI( TAG, "LED state set to: %s", led_state ? "ON" : "OFF" );

  // Create response using telemetry object
  telemetry_t* response_tel = telemetry_create();
  if ( response_tel )
  {
    telemetry_add_string( response_tel, "status", "success" );
    telemetry_add_boolean( response_tel, "led_state", led_state );

    char* json = telemetry_to_json( response_tel );
    if ( json )
    {
      size_t len = strlen( json );
      if ( len < response_max_len )
      {
        strncpy( response_data, json, response_max_len - 1 );
        response_data[response_max_len - 1] = '\0';
        free( json );
        telemetry_free( response_tel );
        return len;
      }
      free( json );
    }
    telemetry_free( response_tel );
  }

  return 0;
}

/**
 * @brief   RPC callback for getStatus method.
 */
static size_t rpc_get_status_callback( const char* request_data, size_t request_len,
                                       char* response_data, size_t response_max_len )
{
  ESP_LOGI( TAG, "RPC getStatus called" );

  // Create status response using telemetry object
  telemetry_t* response_tel = telemetry_create();
  if ( response_tel )
  {
    telemetry_add_double( response_tel, "temperature", temperature );
    telemetry_add_double( response_tel, "humidity", humidity );
    telemetry_add_boolean( response_tel, "led_state", led_state );
    telemetry_add_long( response_tel, "counter", counter );

    char* json = telemetry_to_json( response_tel );
    if ( json )
    {
      size_t len = strlen( json );
      if ( len < response_max_len )
      {
        strncpy( response_data, json, response_max_len - 1 );
        response_data[response_max_len - 1] = '\0';
        free( json );
        telemetry_free( response_tel );
        return len;
      }
      free( json );
    }
    telemetry_free( response_tel );
  }

  return 0;
}

/**
 * @brief   RPC callback for setTemperature method.
 */
static size_t rpc_set_temperature_callback( const char* request_data, size_t request_len,
                                            char* response_data, size_t response_max_len )
{
  ESP_LOGI( TAG, "RPC setTemperature called with data: %.*s", (int) request_len, request_data );

  // Simple parsing - look for "temp":<value>
  char* temp_start = strstr( request_data, "\"temp\":" );
  if ( temp_start )
  {
    temp_start += 7;    // Skip "temp":
    float new_temp = atof( temp_start );
    if ( new_temp > 0 && new_temp < 100 )
    {
      temperature = new_temp;
      ESP_LOGI( TAG, "Temperature set to: %.1f", temperature );
    }
  }

  // Create response using telemetry object
  telemetry_t* response_tel = telemetry_create();
  if ( response_tel )
  {
    telemetry_add_string( response_tel, "status", "success" );
    telemetry_add_double( response_tel, "temperature", temperature );

    char* json = telemetry_to_json( response_tel );
    if ( json )
    {
      size_t len = strlen( json );
      if ( len < response_max_len )
      {
        strncpy( response_data, json, response_max_len - 1 );
        response_data[response_max_len - 1] = '\0';
        free( json );
        telemetry_free( response_tel );
        return len;
      }
      free( json );
    }
    telemetry_free( response_tel );
  }

  return 0;
}

// Shared Attribute Callback

/**
 * @brief   Shared attribute update callback.
 */
static void shared_attribute_callback( const char* data, size_t data_len )
{
  ESP_LOGI( TAG, "Shared attribute update received: %.*s", (int) data_len, data );

  // Simple parsing for demonstration
  if ( strstr( data, SHARED_ATTR_FIRMWARE_VERSION ) )
  {
    ESP_LOGI( TAG, "Firmware version attribute updated" );
  }
  if ( strstr( data, SHARED_ATTR_CONFIG_INTERVAL ) )
  {
    ESP_LOGI( TAG, "Configuration interval attribute updated" );
  }
  if ( strstr( data, SHARED_ATTR_DEBUG_MODE ) )
  {
    ESP_LOGI( TAG, "Debug mode attribute updated" );
  }
}

/**
 * @brief   Setup RPC subscriptions.
 */
static bool setup_rpc_subscriptions( void )
{
  rpc_callback_t rpc_callbacks[3];

  // Setup setLED RPC
  if ( !rpc_callback_create( &rpc_callbacks[0], RPC_SET_LED_METHOD, rpc_set_led_callback, 256 ) )
  {
    ESP_LOGE( TAG, "Failed to create setLED RPC callback" );
    return false;
  }

  // Setup getStatus RPC
  if ( !rpc_callback_create( &rpc_callbacks[1], RPC_GET_STATUS_METHOD, rpc_get_status_callback, 512 ) )
  {
    ESP_LOGE( TAG, "Failed to create getStatus RPC callback" );
    return false;
  }

  // Setup setTemperature RPC
  if ( !rpc_callback_create( &rpc_callbacks[2], RPC_SET_TEMPERATURE_METHOD, rpc_set_temperature_callback, 256 ) )
  {
    ESP_LOGE( TAG, "Failed to create setTemperature RPC callback" );
    return false;
  }

  // Subscribe to RPC methods
  for ( int i = 0; i < 3; i++ )
  {
    if ( !thingsboard_subscribe_rpc( &tb_client, &rpc_callbacks[i] ) )
    {
      ESP_LOGE( TAG, "Failed to subscribe to RPC method: %s", rpc_callback_get_name( &rpc_callbacks[i] ) );
      return false;
    }
  }

  ESP_LOGI( TAG, "RPC subscriptions setup successfully" );
  return true;
}

/**
 * @brief   Setup shared attribute subscriptions.
 */
static bool setup_shared_attribute_subscriptions( void )
{
  shared_attribute_callback_t shared_callback;

  // Initialize shared attribute callback
  if ( !shared_attribute_callback_init( &shared_callback ) )
  {
    ESP_LOGE( TAG, "Failed to initialize shared attribute callback" );
    return false;
  }

  // Set callback function
  if ( !shared_attribute_callback_set_function( &shared_callback, shared_attribute_callback ) )
  {
    ESP_LOGE( TAG, "Failed to set shared attribute callback function" );
    return false;
  }

  // Add attributes to monitor
  shared_attribute_callback_add_attribute( &shared_callback, SHARED_ATTR_FIRMWARE_VERSION );
  shared_attribute_callback_add_attribute( &shared_callback, SHARED_ATTR_CONFIG_INTERVAL );
  shared_attribute_callback_add_attribute( &shared_callback, SHARED_ATTR_DEBUG_MODE );

  // Subscribe to shared attributes
  if ( !thingsboard_subscribe_shared_attributes( &tb_client, &shared_callback ) )
  {
    ESP_LOGE( TAG, "Failed to subscribe to shared attributes" );
    return false;
  }

  ESP_LOGI( TAG, "Shared attribute subscriptions setup successfully" );
  return true;
}

/**
 * @brief   Send telemetry data.
 */
static void send_telemetry_data( void )
{
  ESP_LOGI( TAG, "Sending telemetry data..." );

  // Update simulated sensor values
  temperature += (float) ( rand() % 20 - 10 ) / 10.0f;    // ±1.0°C variation
  humidity += (float) ( rand() % 10 - 5 ) / 10.0f;    // ±0.5% variation
  counter++;

  // Ensure reasonable ranges
  if ( temperature < 15.0f )
    temperature = 15.0f;
  if ( temperature > 35.0f )
    temperature = 35.0f;
  if ( humidity < 30.0f )
    humidity = 30.0f;
  if ( humidity > 90.0f )
    humidity = 90.0f;

  // Create telemetry object and send multiple values at once
  telemetry_t* tel = telemetry_create();
  if ( tel )
  {
    telemetry_add_double( tel, "temperature", temperature );
    telemetry_add_double( tel, "humidity", humidity );
    telemetry_add_boolean( tel, "led_state", led_state );
    telemetry_add_long( tel, "counter", counter );
    telemetry_add_string( tel, "device_type", "ESP32_TEST" );
    telemetry_add_double( tel, "uptime", (double) esp_timer_get_time() / 1000000.0 );
    telemetry_add_long( tel, "free_heap", esp_get_free_heap_size() );

    char* json = telemetry_to_json(tel);
    if (json) {
      printf("Sending telemetry: %s\n", json);
      free(json);
    }

    if ( thingsboard_send_telemetry_data( &tb_client, tel ) )
    {
      ESP_LOGI( TAG, "Telemetry sent - Temp: %.1f°C, Humidity: %.1f%%, LED: %s, Counter: %d",
                temperature, humidity, led_state ? "ON" : "OFF", counter );
    }
    else
    {
      ESP_LOGE( TAG, "Failed to send telemetry data" );
    }

    telemetry_free( tel );
  }
}

/**
 * @brief   Send attribute data.
 */
static void send_attribute_data( void )
{
  ESP_LOGI( TAG, "Sending attribute data..." );

  // Create attributes object
  telemetry_t* attr = telemetry_create();
  if ( attr )
  {
    telemetry_add_string( attr, "device_model", "ESP32-DevKitC" );
    telemetry_add_string( attr, "firmware_version", "1.0.0" );
    telemetry_add_boolean( attr, "telemetry_enabled", true );
    telemetry_add_string( attr, "manufacturer", "Espressif" );
    telemetry_add_long( attr, "flash_size", 4194304 );    // 4MB
    telemetry_add_string( attr, "sdk_version", esp_get_idf_version() );

    if ( thingsboard_send_attributes( &tb_client, attr ) )
    {
      ESP_LOGI( TAG, "Attributes sent successfully" );
    }
    else
    {
      ESP_LOGE( TAG, "Failed to send attributes" );
    }

    telemetry_free( attr );
  }
}

/**
 * @brief   ThingsBoard client task.
 * @param   [in] pvParameters - Task parameters.
 * @return  None
 */
static void thingsboard_task( void* pvParameters )
{
  uint32_t last_telemetry_time = 0;
  uint32_t last_attribute_time = 0;

  ESP_LOGI( TAG, "ThingsBoard task started" );

  while ( 1 )
  {
    uint32_t current_time = pdTICKS_TO_MS( xTaskGetTickCount() );

    // Connect to ThingsBoard if not connected
    if ( !tb_connected )
    {
      ESP_LOGI( TAG, "Connecting to ThingsBoard..." );
      if ( thingsboard_connect( &tb_client, TB_SERVER, TB_TOKEN, TB_PORT ) )
      {
        tb_connected = true;
        ESP_LOGI( TAG, "Connected to ThingsBoard successfully" );
      }
      else
      {
        ESP_LOGE( TAG, "Failed to connect to ThingsBoard" );
        vTaskDelay( pdMS_TO_TICKS( 5000 ) );
        continue;
      }
    }

    // Check if we're still connected
    if ( !thingsboard_is_connected( &tb_client ) )
    {
      tb_connected = false;
      subscriptions_active = false;
      ESP_LOGW( TAG, "ThingsBoard connection lost" );
      vTaskDelay( pdMS_TO_TICKS( 1000 ) );
      continue;
    }

    // Setup subscriptions once after connection
    // if ( tb_connected && !subscriptions_active )
    // {
    //   if ( setup_rpc_subscriptions() && setup_shared_attribute_subscriptions() )
    //   {
    //     subscriptions_active = true;
    //     ESP_LOGI( TAG, "All subscriptions setup successfully" );
    //   }
    //   else
    //   {
    //     ESP_LOGE( TAG, "Failed to setup subscriptions" );
    //     vTaskDelay( pdMS_TO_TICKS( 5000 ) );
    //     continue;
    //   }
    // }

    // Send telemetry data periodically
    if ( current_time - last_telemetry_time >= TELEMETRY_SEND_INTERVAL_MS || last_telemetry_time == 0 )
    {
      send_telemetry_data();
      last_telemetry_time = current_time;
    }

    // Send attribute data periodically
    // if ( current_time - last_attribute_time >= ATTRIBUTE_SEND_INTERVAL_MS )
    // {
    //   send_attribute_data();
    //   last_attribute_time = current_time;
    // }

    // Small delay to prevent excessive CPU usage
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

/**
 * @brief   Application main function.
 * @return  None
 */
void test_thing_board( void )
{
  ESP_LOGI( TAG, "ThingsBoard Test Application Starting..." );
  ESP_LOGI( TAG, "Free memory: %lu bytes", esp_get_free_heap_size() );
  ESP_LOGI( TAG, "IDF version: %s", esp_get_idf_version() );

  // Initialize ThingsBoard client
  if ( !thingsboard_init( &tb_client, 2048, 2048, 4096 ) )
  {
    ESP_LOGE( TAG, "Failed to initialize ThingsBoard client" );
    return;
  }

  // Start ThingsBoard processing task
  if ( !thingsboard_start_task( &tb_client, 4096, 5, "TB_Process" ) )
  {
    ESP_LOGE( TAG, "Failed to start ThingsBoard processing task" );
    return;
  }

  // Create main application task
  xTaskCreate( thingsboard_task, "TB_Main", 8192, NULL, 6, NULL );

  ESP_LOGI( TAG, "ThingsBoard Test Application started successfully" );
}
