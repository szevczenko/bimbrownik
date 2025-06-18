#include "thingsboard.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_app.h"
#include "mqtt_config.h"

// Global ThingsBoard instance for task management
static thingsboard_t* g_thingsboard_instance = NULL;

// ===== PRIVATE HELPER FUNCTIONS =====

static bool validate_thingsboard_instance( const thingsboard_t* tb )
{
  return tb && tb->initialized;
}

static bool validate_connection( const thingsboard_t* tb )
{
  return validate_thingsboard_instance( tb ) && thingsboard_is_connected( tb );
}

static bool change_mqtt_config(const char* host, uint16_t port, const char* client_id, const char* username, const char* password) {
    // Update MQTT configuration with new connection parameters
    
    // Set server address (host:port format)
    char server_address[256];
    snprintf(server_address, sizeof(server_address), "mqtt://%s:%u", host, port);
    
    printf("Configuring MQTT:\n");
    printf("  Address: %s\n", server_address);
    printf("  Client ID: %s\n", client_id ? client_id : "(null)");
    printf("  Username: %s\n", username ? username : "(null)");
    printf("  Password: %s\n", password ? "***" : "(null)");
    
    // Configure MQTT connection parameters
    if (!MQTTConfig_SetString(server_address, MQTT_CONFIG_VALUE_ADDRESS)) {
        printf("Failed to set MQTT address\n");
        return false;
    }
    
    // Set client ID (use empty string if NULL)
    const char* safe_client_id = client_id ? client_id : "";
    if (!MQTTConfig_SetString(safe_client_id, MQTT_CONFIG_VALUE_CLIENT_ID)) {
        printf("Failed to set MQTT client ID\n");
        return false;
    }
    
    // Set username (use empty string if NULL)
    const char* safe_username = username ? username : "";
    if (!MQTTConfig_SetString(safe_username, MQTT_CONFIG_VALUE_USERNAME)) {
        printf("Failed to set MQTT username\n");
        return false;
    }
    
    // Set password (use empty string if NULL)
    const char* safe_password = password ? password : "";
    if (!MQTTConfig_SetString(safe_password, MQTT_CONFIG_VALUE_PASSWORD)) {
        printf("Failed to set MQTT password\n");
        return false;
    }
    
    printf("MQTT configuration updated successfully\n");
    return true;
}

// ===== TASK MANAGEMENT =====

static void thingsboard_task( void* pvParameters )
{
  thingsboard_t* tb = (thingsboard_t*) pvParameters;

  if ( !validate_thingsboard_instance( tb ) )
  {
    printf( "ThingsBoard task: Invalid parameter\n" );
    vTaskDelete( NULL );
    return;
  }

  printf( "ThingsBoard processing task started\n" );

  while ( tb->task_running )
  {
    uint32_t current_time_ms = pdTICKS_TO_MS( xTaskGetTickCount() );

    // Update timeouts for pending attribute requests
    attributes_update_timeouts( &tb->attributes_manager, current_time_ms );

    // Add delay to prevent excessive CPU usage
    vTaskDelay( pdMS_TO_TICKS( 100 ) );
  }

  printf( "ThingsBoard processing task stopped\n" );

  // Clear task handle before deleting
  tb->task_handle = NULL;
  vTaskDelete( NULL );
}

// ===== CORE FUNCTIONS =====

bool thingsboard_init( thingsboard_t* tb, uint16_t receive_buffer_size, uint16_t send_buffer_size, size_t max_stack_size )
{
  if ( !tb )
  {
    return false;
  }

  memset( tb, 0, sizeof( thingsboard_t ) );

  // Initialize component managers
  if ( !attributes_init( &tb->attributes_manager ) || !shared_attribute_update_init( &tb->shared_attr_manager ) || !server_side_rpc_init( &tb->rpc_manager ) )
  {
    return false;
  }

  // Set configuration
  tb->receive_buffer_size = receive_buffer_size;
  tb->send_buffer_size = send_buffer_size;
  tb->max_stack_size = max_stack_size;
  tb->request_id_counter = 1;
  tb->task_handle = NULL;
  tb->task_running = false;

  // Set global instance for task management
  g_thingsboard_instance = tb;
  tb->initialized = true;

  printf( "ThingsBoard client initialized\n" );
  return true;
}

static bool wait_mqtt_connected( thingsboard_t* tb, uint32_t timeout_ms )
{
  uint32_t start_time = pdTICKS_TO_MS( xTaskGetTickCount() );
  while ( !MqttApp_IsConnected() )
  {
    if ( pdTICKS_TO_MS( xTaskGetTickCount() ) - start_time >= timeout_ms )
    {
      return false;    // Timeout
    }
    vTaskDelay( pdMS_TO_TICKS( 100 ) );    // Wait before retrying
  }
  return true;
}

bool thingsboard_connect( thingsboard_t* tb, const char* host, const char* access_token, uint16_t port )
{
  if ( !validate_thingsboard_instance( tb ) || !host || !access_token )
  {
    printf("Invalid parameters for ThingsBoard connection\n");
    return false;
  }
  
  printf("ThingsBoard connecting to %s:%u with token %s\n", host, port, access_token);
  
  // Store connection parameters
  strncpy( tb->server_host, host, sizeof( tb->server_host ) - 1 );
  tb->server_host[sizeof( tb->server_host ) - 1] = '\0';

  strncpy( tb->access_token, access_token, sizeof( tb->access_token ) - 1 );
  tb->access_token[sizeof( tb->access_token ) - 1] = '\0';

  // Use access token as client_id for ThingsBoard standard mode
  strncpy( tb->client_id, access_token, sizeof( tb->client_id ) - 1 );
  tb->client_id[sizeof( tb->client_id ) - 1] = '\0';

  tb->server_port = port;

  // Configure MQTT to use access token as username (ThingsBoard standard mode)
  // client_id = access_token, username = access_token, password = empty
  if ( !change_mqtt_config( host, port, access_token, access_token, "" ) )
  {
    printf( "Failed to configure MQTT settings\n" );
    return false;
  }

  // Wait a bit for config to be applied
  vTaskDelay(pdMS_TO_TICKS(100));
  
  // Initialize MQTT if not already done
  if (!MqttApp_IsConnected()) {
      
        // Wait for connection
        for (int i = 0; i < 50; i++) { // Wait up to 5 seconds
            if (MqttApp_IsConnected()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    if (!MqttApp_IsConnected()) {
        printf("Failed to establish MQTT connection\n");
        return false;
    }
    
  printf( "Connected to ThingsBoard server: %s:%u (using access token as username)\n", host, port );
  return true;
}

bool thingsboard_connect_with_credentials( thingsboard_t* tb, const char* host, uint16_t port, const char* client_id, const char* username, const char* password )
{
  if ( !validate_thingsboard_instance( tb ) || !host || !client_id || !username )
  {
    return false;
  }

  // Store connection parameters
  strncpy( tb->server_host, host, sizeof( tb->server_host ) - 1 );
  tb->server_host[sizeof( tb->server_host ) - 1] = '\0';

  strncpy( tb->client_id, client_id, sizeof( tb->client_id ) - 1 );
  tb->client_id[sizeof( tb->client_id ) - 1] = '\0';

  // Store username as access token for internal reference
  strncpy( tb->access_token, username, sizeof( tb->access_token ) - 1 );
  tb->access_token[sizeof( tb->access_token ) - 1] = '\0';

  tb->server_port = port;

  // Configure MQTT with explicit credentials (custom authentication mode)
  if ( !change_mqtt_config( host, port, client_id, username, password ) )
  {
    printf( "Failed to configure MQTT settings\n" );
    return false;
  }

  wait_mqtt_connected( tb, 5000 );    // Wait for MQTT connection

  printf( "Connected to ThingsBoard server: %s:%u (using custom credentials)\n", host, port );
  return true;
}

void thingsboard_disconnect( thingsboard_t* tb )
{
  if ( !validate_thingsboard_instance( tb ) )
  {
    return;
  }

  // Unsubscribe from all topics
  attributes_unsubscribe_all( &tb->attributes_manager );
  shared_attribute_update_unsubscribe_all( &tb->shared_attr_manager );
  server_side_rpc_unsubscribe_all( &tb->rpc_manager );

  printf( "Disconnected from ThingsBoard server\n" );
}

bool thingsboard_is_connected( const thingsboard_t* tb )
{
  return validate_thingsboard_instance( tb ) && MqttApp_IsConnected();
}

bool thingsboard_start_task( thingsboard_t* tb, uint32_t task_stack_size, UBaseType_t task_priority, const char* task_name )
{
  if ( !validate_thingsboard_instance( tb ) )
  {
    return false;
  }

  if ( tb->task_running && tb->task_handle != NULL )
  {
    printf( "ThingsBoard task already running\n" );
    return true;
  }

  const char* name = task_name ? task_name : "ThingsBoard";
  tb->task_running = true;

  BaseType_t result = xTaskCreate(
    thingsboard_task,
    name,
    task_stack_size / sizeof( StackType_t ),
    tb,
    task_priority,
    &tb->task_handle );

  if ( result != pdPASS )
  {
    printf( "Failed to create ThingsBoard task\n" );
    tb->task_running = false;
    tb->task_handle = NULL;
    return false;
  }

  printf( "ThingsBoard task created successfully\n" );
  return true;
}

bool thingsboard_stop_task( thingsboard_t* tb )
{
  if ( !validate_thingsboard_instance( tb ) )
  {
    return false;
  }

  if ( !tb->task_running || tb->task_handle == NULL )
  {
    return true;
  }

  tb->task_running = false;
  vTaskDelay( pdMS_TO_TICKS( 200 ) );    // Wait for graceful shutdown

  if ( tb->task_handle != NULL )
  {
    vTaskDelete( tb->task_handle );
    tb->task_handle = NULL;
  }

  printf( "ThingsBoard task stopped\n" );
  return true;
}

void thingsboard_cleanup( thingsboard_t* tb )
{
  if ( !validate_thingsboard_instance( tb ) )
  {
    return;
  }

  thingsboard_stop_task( tb );

  // Cleanup component managers
  attributes_cleanup( &tb->attributes_manager );
  shared_attribute_update_cleanup( &tb->shared_attr_manager );
  server_side_rpc_cleanup( &tb->rpc_manager );

  if ( g_thingsboard_instance == tb )
  {
    g_thingsboard_instance = NULL;
  }

  memset( tb, 0, sizeof( thingsboard_t ) );
  printf( "ThingsBoard client cleaned up\n" );
}

// ===== TELEMETRY FUNCTIONS =====

bool thingsboard_send_telemetry_data( thingsboard_t* tb, telemetry_t* telemetry )
{
  if ( !validate_connection( tb ) || !telemetry )
  {
    return false;
  }

  char* json = telemetry_to_json( telemetry );
  if ( !json )
  {
    return false;
  }
  printf( "Sending telemetry: %s\n", json );
  bool success = MqttApp_PostData( TELEMETRY_TOPIC, json, 0 );
  free( json );

  return success;
}

bool thingsboard_send_telemetry_json( thingsboard_t* tb, const char* json )
{
  if ( !validate_connection( tb ) || !json )
  {
    return false;
  }

  return MqttApp_PostData( TELEMETRY_TOPIC, json, 0 );
}

// ===== ATTRIBUTE FUNCTIONS =====

bool thingsboard_send_attributes( thingsboard_t* tb, telemetry_t* attributes )
{
  if ( !validate_connection( tb ) || !attributes )
  {
    return false;
  }

  char* json = telemetry_to_json( attributes );
  if ( !json )
  {
    return false;
  }
  printf( "Sending attributes: %s\n", json );
  bool success = MqttApp_PostData( ATTRIBUTE_TOPIC, json, 0 );
  free( json );

  return success;
}

bool thingsboard_send_attributes_json( thingsboard_t* tb, const char* json )
{
  if ( !validate_connection( tb ) || !json )
  {
    return false;
  }

  return MqttApp_PostData( ATTRIBUTE_TOPIC, json, 0 );
}

// ===== REQUEST FUNCTIONS =====

bool thingsboard_request_client_attributes( thingsboard_t* tb, const char* const* attribute_keys, size_t keys_count, uint32_t timeout_ms, void ( *callback )( const char* topic, const char* data, size_t data_len ) )
{
  if ( !validate_connection( tb ) )
  {
    return false;
  }

  return attributes_request_client( &tb->attributes_manager, attribute_keys, keys_count, timeout_ms, callback );
}

bool thingsboard_request_shared_attributes( thingsboard_t* tb, const char* const* attribute_keys, size_t keys_count, uint32_t timeout_ms, void ( *callback )( const char* topic, const char* data, size_t data_len ) )
{
  if ( !validate_connection( tb ) )
  {
    return false;
  }

  return attributes_request_shared( &tb->attributes_manager, attribute_keys, keys_count, timeout_ms, callback );
}

// ===== SUBSCRIPTION FUNCTIONS =====

bool thingsboard_subscribe_shared_attributes( thingsboard_t* tb, const shared_attribute_callback_t* callback )
{
  if ( !validate_thingsboard_instance( tb ) || !callback )
  {
    return false;
  }

  return shared_attribute_update_subscribe_single( &tb->shared_attr_manager, callback );
}

bool thingsboard_subscribe_rpc( thingsboard_t* tb, const rpc_callback_t* callback )
{
  if ( !validate_thingsboard_instance( tb ) || !callback )
  {
    return false;
  }

  return server_side_rpc_subscribe_single( &tb->rpc_manager, callback );
}

// ===== UTILITY FUNCTIONS =====

bool thingsboard_send_claim_request( thingsboard_t* tb, const char* secret_key, uint32_t duration_ms )
{
  if ( !validate_connection( tb ) )
  {
    return false;
  }

  telemetry_t* tel = telemetry_create();
  if ( !tel )
  {
    return false;
  }

  bool success = true;

  if ( secret_key && strlen( secret_key ) > 0 )
  {
    success = ( telemetry_add_string( tel, "secretKey", secret_key ) == 0 );
  }

  if ( success )
  {
    success = ( telemetry_add_long( tel, "durationMs", (long) duration_ms ) == 0 );
  }

  if ( success )
  {
    char* json = telemetry_to_json( tel );
    if ( json )
    {
      success = MqttApp_PostData( "v1/devices/me/claim", json, 0 );
      free( json );
    }
    else
    {
      success = false;
    }
  }

  telemetry_free( tel );
  return success;
}

bool thingsboard_set_buffer_size( thingsboard_t* tb, uint16_t receive_buffer_size, uint16_t send_buffer_size )
{
  if ( !validate_thingsboard_instance( tb ) )
  {
    return false;
  }

  tb->receive_buffer_size = receive_buffer_size;
  tb->send_buffer_size = send_buffer_size;

  return true;
}
