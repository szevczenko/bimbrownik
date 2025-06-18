#include "server_side_rpc.h"
#include "mqtt_app.h"
#include "telemetry.h"
#include "mongoose.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

// Server side RPC topic constants
const char RPC_SUBSCRIBE_TOPIC[] = "v1/devices/me/rpc/request/+";
const char RPC_REQUEST_TOPIC[] = "v1/devices/me/rpc/request/";
const char RPC_SEND_RESPONSE_TOPIC[] = "v1/devices/me/rpc/response/%lu";

// JSON key constants
const char RPC_METHOD_KEY[] = "method";
const char RPC_PARAMS_KEY[] = "params";

// Global manager for callback handling
static server_side_rpc_t* g_rpc_manager = NULL;

// MQTT callback function for server-side RPC requests
static void server_side_rpc_mqtt_callback(const char* topic, const char* data, size_t data_len) {
    if (g_rpc_manager) {
        server_side_rpc_process_request(g_rpc_manager, topic, data, data_len);
    }
}

static bool ensure_topic_subscription(server_side_rpc_t* manager) {
    if (!manager->subscribed_to_topic && manager->callback_count > 0) {
        if (MqttApp_Subscribe(RPC_SUBSCRIBE_TOPIC, 0, server_side_rpc_mqtt_callback, 5000)) {
            manager->subscribed_to_topic = true;
            printf("Subscribed to server-side RPC topic\n");
            return true;
        } else {
            printf("Failed to subscribe to server-side RPC topic\n");
            return false;
        }
    }
    return true;
}

bool server_side_rpc_init(server_side_rpc_t* manager) {
    if (!manager) {
        return false;
    }
    
    memset(manager, 0, sizeof(server_side_rpc_t));
    manager->callback_count = 0;
    manager->subscribed_to_topic = false;
    g_rpc_manager = manager;
    
    return true;
}

bool server_side_rpc_subscribe_single(server_side_rpc_t* manager, const rpc_callback_t* callback) {
    if (!manager || !callback) {
        printf("Invalid parameters for server-side RPC subscription\n");
        return false;
    }
    
    if (manager->callback_count >= MAX_SERVER_SIDE_RPC_SUBSCRIPTIONS) {
        printf("Maximum server-side RPC subscriptions reached\n");
        return false;
    }
    
    if (!rpc_callback_is_active(callback)) {
        printf("Invalid RPC callback provided\n");
        return false;
    }
    
    // Copy the callback to our internal storage
    memcpy(&manager->callbacks[manager->callback_count], callback, sizeof(rpc_callback_t));
    manager->callback_count++;
    
    // Ensure we're subscribed to the MQTT topic
    if (!ensure_topic_subscription(manager)) {
        // Rollback the addition if subscription failed
        manager->callback_count--;
        printf("Failed to ensure topic subscription for RPC method: %s\n", rpc_callback_get_name(callback));
        return false;
    }
    
    printf("Added server-side RPC subscription for method: %s (total: %zu)\n",
           rpc_callback_get_name(callback), manager->callback_count);
    return true;
}

bool server_side_rpc_subscribe_multiple(server_side_rpc_t* manager,
                                       const rpc_callback_t* callbacks,
                                       size_t count) {
    if (!manager || !callbacks || count == 0) {
        return false;
    }
    
    if (manager->callback_count + count > MAX_SERVER_SIDE_RPC_SUBSCRIPTIONS) {
        printf("Not enough space for %zu additional RPC subscriptions\n", count);
        return false;
    }
    
    // Copy all callbacks to our internal storage
    for (size_t i = 0; i < count; i++) {
        if (!rpc_callback_is_active(&callbacks[i])) {
            printf("Invalid RPC callback at index %zu\n", i);
            return false;
        }
        memcpy(&manager->callbacks[manager->callback_count + i], &callbacks[i], sizeof(rpc_callback_t));
    }
    manager->callback_count += count;
    
    // Ensure we're subscribed to the MQTT topic
    if (!ensure_topic_subscription(manager)) {
        // Rollback all additions if subscription failed
        manager->callback_count -= count;
        return false;
    }
    
    printf("Added %zu server-side RPC subscriptions (total: %zu)\n", count, manager->callback_count);
    return true;
}

bool server_side_rpc_unsubscribe_all(server_side_rpc_t* manager) {
    if (!manager) {
        return false;
    }
    
    // Clear all callbacks
    for (size_t i = 0; i < manager->callback_count; i++) {
        rpc_callback_cleanup(&manager->callbacks[i]);
    }
    manager->callback_count = 0;
    
    // Unsubscribe from MQTT topic
    bool success = true;
    if (manager->subscribed_to_topic) {
        success = MqttApp_Unsubscribe(RPC_SUBSCRIBE_TOPIC, 5000);
        manager->subscribed_to_topic = false;
        if (success) {
            printf("Unsubscribed from server-side RPC topic\n");
        } else {
            printf("Failed to unsubscribe from server-side RPC topic\n");
        }
    }
    
    return success;
}

void server_side_rpc_process_request(server_side_rpc_t* manager,
                                    const char* topic,
                                    const char* data,
                                    size_t data_len) {
    if (!manager || !topic || !data) {
        return;
    }
    
    // Check if this is a server-side RPC request topic
    if (!server_side_rpc_compare_topic(topic)) {
        return;
    }
    
    printf("Processing server-side RPC request\n");
    
    // Parse JSON to extract method name
    struct mg_str json_str = mg_str_n(data, data_len);
    char* method_name = mg_json_get_str(json_str, "$.method");
    if (!method_name) {
        printf("No method name found in RPC request\n");
        return;
    }
    
    printf("RPC method requested: %s\n", method_name);
    
    // Find matching callback
    rpc_callback_t* matching_callback = NULL;
    for (size_t i = 0; i < manager->callback_count; i++) {
        if (rpc_callback_matches_method(&manager->callbacks[i], method_name)) {
            matching_callback = &manager->callbacks[i];
            break;
        }
    }
    
    if (!matching_callback) {
        printf("No callback found for RPC method: %s\n", method_name);
        free(method_name);
        return;
    }
    
    // Extract parameters (optional)
    struct mg_str params_data = {0};
    int params_len;
    int params_offset = mg_json_get(json_str, "$.params", &params_len);
    if (params_offset >= 0) {
        params_data = mg_str_n(data + params_offset, params_len);
    }
    
    // Prepare response buffer
    char response_buffer[MAX_RPC_RESPONSE_SIZE];
    memset(response_buffer, 0, sizeof(response_buffer));
    
    // Call the RPC callback
    size_t response_len = rpc_callback_call(matching_callback,
                                          params_data.buf,
                                          params_data.len,
                                          response_buffer,
                                          sizeof(response_buffer));
    
    // Send response if there is one
    if (response_len > 0) {
        uint32_t request_id = server_side_rpc_parse_request_id(topic);
        if (request_id > 0) {
            server_side_rpc_send_response(request_id, response_buffer, response_len);
        }
    }
    
    free(method_name);
}

bool server_side_rpc_compare_topic(const char* topic) {
    if (!topic) {
        return false;
    }
    
    return strncmp(RPC_REQUEST_TOPIC, topic, strlen(RPC_REQUEST_TOPIC)) == 0;
}

bool server_side_rpc_resubscribe(server_side_rpc_t* manager) {
    if (!manager) {
        return false;
    }
    
    if (manager->callback_count > 0 && !manager->subscribed_to_topic) {
        return ensure_topic_subscription(manager);
    }
    
    return true;
}

size_t server_side_rpc_get_subscription_count(const server_side_rpc_t* manager) {
    if (!manager) {
        return 0;
    }
    
    return manager->callback_count;
}

bool server_side_rpc_has_subscriptions(const server_side_rpc_t* manager) {
    if (!manager) {
        return false;
    }
    
    return manager->callback_count > 0;
}

uint32_t server_side_rpc_parse_request_id(const char* topic) {
    if (!topic) {
        return 0;
    }
    
    const char* id_start = strrchr(topic, '/');
    if (id_start) {
        return (uint32_t)atoi(id_start + 1);
    }
    return 0;
}

bool server_side_rpc_send_response(uint32_t request_id, const char* response_data, size_t response_len) {
    if (!response_data || response_len == 0) {
        return false;
    }
    
    char response_topic[128];
    snprintf(response_topic, sizeof(response_topic), RPC_SEND_RESPONSE_TOPIC, request_id);
    
    // Ensure response is null-terminated
    char* null_terminated_response = malloc(response_len + 1);
    if (!null_terminated_response) {
        printf("Failed to allocate memory for RPC response\n");
        return false;
    }
    
    memcpy(null_terminated_response, response_data, response_len);
    null_terminated_response[response_len] = '\0';
    
    bool success = MqttApp_PostData(response_topic, null_terminated_response, 0);
    free(null_terminated_response);
    
    if (success) {
        printf("RPC response sent for request ID: %" PRIu32 "\n", request_id);
    } else {
        printf("Failed to send RPC response for request ID: %" PRIu32 "\n", request_id);
    }
    
    return success;
}

void server_side_rpc_cleanup(server_side_rpc_t* manager) {
    if (!manager) {
        return;
    }
    
    server_side_rpc_unsubscribe_all(manager);
    
    if (g_rpc_manager == manager) {
        g_rpc_manager = NULL;
    }
    
    memset(manager, 0, sizeof(server_side_rpc_t));
}
