#ifndef THINGSBOARD_H
#define THINGSBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "constants.h"
#include "telemetry.h"
#include "attributes.h"
#include "shared_attribute_update.h"
#include "server_side_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_MQTT_PORT 1883
#define PROV_ACCESS_TOKEN "provision"

/// @brief Main ThingsBoard client structure that manages all components
typedef struct {
    // Component managers
    attributes_manager_t attributes_manager;
    shared_attribute_update_t shared_attr_manager;
    server_side_rpc_t rpc_manager;
    
    // Client state
    bool initialized;
    uint32_t request_id_counter;
    
    // Buffer settings
    uint16_t receive_buffer_size;
    uint16_t send_buffer_size;
    size_t max_stack_size;
    
    // Connection settings
    char server_host[128];
    uint16_t server_port;
    char access_token[64];
    char client_id[64];
    
    // Task management
    TaskHandle_t task_handle;
    bool task_running;
} thingsboard_t;

// Core functions
bool thingsboard_init(thingsboard_t* tb, uint16_t receive_buffer_size, uint16_t send_buffer_size, size_t max_stack_size);
bool thingsboard_connect(thingsboard_t* tb, const char* host, const char* access_token, uint16_t port);
bool thingsboard_connect_with_credentials(thingsboard_t* tb, const char* host, uint16_t port, const char* client_id, const char* username, const char* password);
void thingsboard_disconnect(thingsboard_t* tb);
bool thingsboard_is_connected(const thingsboard_t* tb);
bool thingsboard_start_task(thingsboard_t* tb, uint32_t task_stack_size, UBaseType_t task_priority, const char* task_name);
bool thingsboard_stop_task(thingsboard_t* tb);
void thingsboard_cleanup(thingsboard_t* tb);

// Telemetry functions
bool thingsboard_send_telemetry_data(thingsboard_t* tb, telemetry_t* telemetry);
bool thingsboard_send_telemetry_json(thingsboard_t* tb, const char* json);

// Attribute functions  
bool thingsboard_send_attributes(thingsboard_t* tb, telemetry_t* attributes);
bool thingsboard_send_attributes_json(thingsboard_t* tb, const char* json);

// Request functions
bool thingsboard_request_client_attributes(thingsboard_t* tb, const char* const* attribute_keys, size_t keys_count, uint32_t timeout_ms, void (*callback)(const char* topic, const char* data, size_t data_len));
bool thingsboard_request_shared_attributes(thingsboard_t* tb, const char* const* attribute_keys, size_t keys_count, uint32_t timeout_ms, void (*callback)(const char* topic, const char* data, size_t data_len));

// Subscription functions
bool thingsboard_subscribe_shared_attributes(thingsboard_t* tb, const shared_attribute_callback_t* callback);
bool thingsboard_subscribe_rpc(thingsboard_t* tb, const rpc_callback_t* callback);

// Utility functions
bool thingsboard_send_claim_request(thingsboard_t* tb, const char* secret_key, uint32_t duration_ms);
bool thingsboard_set_buffer_size(thingsboard_t* tb, uint16_t receive_buffer_size, uint16_t send_buffer_size);

#ifdef __cplusplus
}
#endif

#endif // THINGSBOARD_H
