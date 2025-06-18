#include "attributes.h"
#include "mqtt_app.h"
#include "telemetry.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// MQTT topic constants
const char ATTRIBUTE_REQUEST_TOPIC[] = "v1/devices/me/attributes/request/%u";
const char ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC[] = "v1/devices/me/attributes/response/+";
const char ATTRIBUTE_RESPONSE_TOPIC[] = "v1/devices/me/attributes/response/";

// Request/response key constants
const char CLIENT_REQUEST_KEYS[] = "clientKeys";
const char CLIENT_RESPONSE_KEY[] = "client";
const char SHARED_REQUEST_KEY[] = "sharedKeys";
const char SHARED_RESPONSE_KEY[] = "shared";

// Helper function to get current time in milliseconds
static uint32_t get_current_time_ms(void) {
    return pdTICKS_TO_MS(xTaskGetTickCount());
}

// Helper function to parse request ID from topic
static uint32_t parse_request_id_from_topic(const char* topic) {
    const char* id_start = strrchr(topic, '/');
    if (id_start) {
        return (uint32_t)atoi(id_start + 1);
    }
    return 0;
}

// Helper function to find free request slot
static attribute_request_t* find_free_request_slot(attributes_manager_t* manager) {
    for (size_t i = 0; i < MAX_ATTRIBUTE_SUBSCRIPTIONS; i++) {
        if (!manager->requests[i].active) {
            return &manager->requests[i];
        }
    }
    return NULL;
}

// Helper function to find request by ID
static attribute_request_t* find_request_by_id(attributes_manager_t* manager, uint32_t request_id) {
    for (size_t i = 0; i < MAX_ATTRIBUTE_SUBSCRIPTIONS; i++) {
        if (manager->requests[i].active && manager->requests[i].request_id == request_id) {
            return &manager->requests[i];
        }
    }
    return NULL;
}

// Helper function to build attribute keys string
static bool build_keys_string(const char* const* keys, size_t count, char* output, size_t output_size) {
    if (!keys || count == 0 || !output) {
        return false;
    }
    
    output[0] = '\0';
    size_t remaining = output_size - 1;
    
    for (size_t i = 0; i < count; i++) {
        if (!keys[i] || strlen(keys[i]) == 0) {
            continue;
        }
        
        size_t key_len = strlen(keys[i]);
        if (key_len + 1 > remaining) { // +1 for comma or null terminator
            return false;
        }
        
        if (strlen(output) > 0) {
            strncat(output, ",", remaining);
            remaining--;
        }
        strncat(output, keys[i], remaining);
        remaining -= key_len;
    }
    
    return strlen(output) > 0;
}

// Helper function to clear request
static void clear_request(attribute_request_t* request) {
    if (request) {
        memset(request, 0, sizeof(attribute_request_t));
        request->active = false;
    }
}

// Global manager for callback handling
static attributes_manager_t* g_attributes_manager = NULL;

// MQTT callback function for attribute responses
static void attributes_mqtt_callback(const char* topic, const char* data, size_t data_len) {
    if (g_attributes_manager) {
        attributes_process_response(g_attributes_manager, topic, data, data_len);
    }
}

bool attributes_init(attributes_manager_t* manager) {
    if (!manager) {
        return false;
    }
    
    memset(manager, 0, sizeof(attributes_manager_t));
    manager->active_count = 0;
    manager->subscribed_to_responses = false;
    manager->subscribed_to_topic = false;
    manager->next_request_id = 1;
    
    // Set global manager reference
    g_attributes_manager = manager;
    
    return true;
}

static bool ensure_topic_subscription(attributes_manager_t* manager) {
    if (!manager->subscribed_to_topic && manager->active_count > 0) {
        if (MqttApp_Subscribe(ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC, 0, attributes_mqtt_callback, 5000)) {
            manager->subscribed_to_topic = true;
            printf("Subscribed to attribute response topic\n");
            return true;
        } else {
            printf("Failed to subscribe to attribute response topic\n");
            return false;
        }
    }
    return true;
}

static bool attributes_request_internal(attributes_manager_t* manager,
                                       const char* const* attribute_keys,
                                       size_t keys_count,
                                       uint32_t timeout_ms,
                                       void (*callback)(const char* topic, const char* data, size_t data_len),
                                       attribute_type_t type) {
    if (!manager || !attribute_keys || keys_count == 0 || !callback) {
        return false;
    }
    
    if (keys_count > MAX_ATTRIBUTES_PER_REQUEST) {
        return false;
    }
    
    // Find free request slot
    attribute_request_t* request = find_free_request_slot(manager);
    if (!request) {
        printf("No free attribute request slots available\n");
        return false;
    }
    
    // Ensure we're subscribed to the response topic
    if (!ensure_topic_subscription(manager)) {
        return false;
    }
    
    // Setup request
    request->active = true;
    request->request_id = manager->next_request_id++;
    request->type = type;
    request->callback = callback;
    request->timeout_ms = timeout_ms;
    request->start_time = get_current_time_ms();
    request->count = 0;
    
    // Copy attribute keys
    for (size_t i = 0; i < keys_count && i < MAX_ATTRIBUTES_PER_REQUEST; i++) {
        if (attribute_keys[i] && strlen(attribute_keys[i]) < MAX_ATTRIBUTE_KEY_LENGTH) {
            strncpy(request->keys[request->count], attribute_keys[i], MAX_ATTRIBUTE_KEY_LENGTH - 1);
            request->keys[request->count][MAX_ATTRIBUTE_KEY_LENGTH - 1] = '\0';
            request->count++;
        }
    }
    
    if (request->count == 0) {
        clear_request(request);
        printf("No valid attribute keys to request\n");
        return false;
    }
    
    // Build keys string for JSON
    char keys_str[256];
    const char* keys_ptrs[MAX_ATTRIBUTES_PER_REQUEST];
    for (size_t i = 0; i < request->count; i++) {
        keys_ptrs[i] = request->keys[i];
    }
    
    if (!build_keys_string(keys_ptrs, request->count, keys_str, sizeof(keys_str))) {
        clear_request(request);
        printf("Failed to build attribute keys string\n");
        return false;
    }
    
    // Build JSON request using telemetry
    telemetry_t* tel = telemetry_create();
    if (!tel) {
        clear_request(request);
        printf("Failed to create telemetry object\n");
        return false;
    }
    
    const char* request_key = (type == ATTRIBUTE_TYPE_CLIENT) ? CLIENT_REQUEST_KEYS : SHARED_REQUEST_KEY;
    if (telemetry_add_string(tel, request_key, keys_str) != 0) {
        telemetry_free(tel);
        clear_request(request);
        printf("Failed to add attribute keys to telemetry\n");
        return false;
    }
    
    char* json = telemetry_to_json(tel);
    telemetry_free(tel);
    
    if (!json) {
        clear_request(request);
        printf("Failed to create JSON request\n");
        return false;
    }
    
    // Build topic
    char topic[MAX_TOPIC_LENGTH];
    snprintf(topic, sizeof(topic), ATTRIBUTE_REQUEST_TOPIC, (unsigned int)request->request_id);
    
    // Send request using mqtt_app
    bool success = MqttApp_PostData(topic, json, 0);
    free(json);
    
    if (!success) {
        clear_request(request);
        printf("Failed to send attribute request\n");
        return false;
    }
    
    manager->active_count++;
    printf("Attribute request sent with ID: %" PRIu32 "\n", request->request_id);
    return true;
}

bool attributes_request_client(attributes_manager_t* manager,
                              const char* const* attribute_keys,
                              size_t keys_count,
                              uint32_t timeout_ms,
                              void (*callback)(const char* topic, const char* data, size_t data_len)) {
    return attributes_request_internal(manager, attribute_keys, keys_count, timeout_ms, callback, ATTRIBUTE_TYPE_CLIENT);
}

bool attributes_request_shared(attributes_manager_t* manager,
                              const char* const* attribute_keys,
                              size_t keys_count,
                              uint32_t timeout_ms,
                              void (*callback)(const char* topic, const char* data, size_t data_len)) {
    return attributes_request_internal(manager, attribute_keys, keys_count, timeout_ms, callback, ATTRIBUTE_TYPE_SHARED);
}

void attributes_process_response(attributes_manager_t* manager, const char* topic, const char* data, size_t data_len) {
    if (!manager || !topic || !data) {
        return;
    }
    
    // Check if this is an attribute response topic
    if (strncmp(ATTRIBUTE_RESPONSE_TOPIC, topic, strlen(ATTRIBUTE_RESPONSE_TOPIC)) != 0) {
        return;
    }
    
    // Parse request ID from topic
    uint32_t request_id = parse_request_id_from_topic(topic);
    if (request_id == 0) {
        printf("Failed to parse request ID from topic: %s\n", topic);
        return;
    }
    
    // Find matching request
    attribute_request_t* request = find_request_by_id(manager, request_id);
    if (!request) {
        printf("No matching request found for ID: %" PRIu32 "\n", request_id);
        return;
    }
    
    printf("Processing attribute response for request ID: %" PRIu32 "\n", request_id);
    
    // Call user callback
    if (request->callback) {
        request->callback(topic, data, data_len);
    }
    
    // Clean up request
    clear_request(request);
    manager->active_count--;
    
    // Unsubscribe if no more active requests
    if (manager->active_count == 0 && manager->subscribed_to_responses) {
        printf("Unsubscribing attributes_process_response\n");
        MqttApp_Unsubscribe(ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC, 5000);
        manager->subscribed_to_responses = false;
    }
}

void attributes_update_timeouts(attributes_manager_t* manager, uint32_t current_time_ms) {
    if (!manager) {
        return;
    }
    
    bool any_expired = false;
    
    for (size_t i = 0; i < MAX_ATTRIBUTE_SUBSCRIPTIONS; i++) {
        attribute_request_t* request = &manager->requests[i];
        
        if (!request->active) {
            continue;
        }
        
        // Check for timeout
        if (current_time_ms - request->start_time >= request->timeout_ms) {
            printf("Attribute request timeout for ID: %" PRIu32 "\n", request->request_id);
            clear_request(request);
            manager->active_count--;
            any_expired = true;
        }
    }
    
    // Only unsubscribe if we had timeouts AND no more active requests
    if (any_expired && manager->active_count == 0 && manager->subscribed_to_topic) {
        MqttApp_Unsubscribe(ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC, 5000);
        manager->subscribed_to_topic = false;
        printf("Unsubscribed from attribute response topic - no more active requests\n");
    }
}

bool attributes_unsubscribe_all(attributes_manager_t* manager) {
    if (!manager) {
        return false;
    }
    
    // Clear all requests
    for (size_t i = 0; i < MAX_ATTRIBUTE_SUBSCRIPTIONS; i++) {
        clear_request(&manager->requests[i]);
    }
    manager->active_count = 0;
    
    // Unsubscribe from response topic using mqtt_app
    if (manager->subscribed_to_responses) {
        printf("Unsubscribing attributes_unsubscribe_all\n");
        bool success = MqttApp_Unsubscribe(ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC, 5000);
        manager->subscribed_to_responses = false;
        return success;
    }
    
    return true;
}

void attributes_cleanup(attributes_manager_t* manager) {
    if (!manager) {
        return;
    }
    
    attributes_unsubscribe_all(manager);
    
    if (g_attributes_manager == manager) {
        g_attributes_manager = NULL;
    }
    
    memset(manager, 0, sizeof(attributes_manager_t));
}
