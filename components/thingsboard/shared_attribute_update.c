#include "shared_attribute_update.h"
#include "mqtt_app.h"
#include "mongoose.h"
#include <string.h>
#include <stdio.h>

// Global manager for callback handling
static shared_attribute_update_t* g_shared_attr_manager = NULL;

// MQTT callback function for shared attribute updates
static void shared_attribute_mqtt_callback(const char* topic, const char* data, size_t data_len) {
    if (g_shared_attr_manager) {
        shared_attribute_update_process_message(g_shared_attr_manager, topic, data, data_len);
    }
}

static bool ensure_topic_subscription(shared_attribute_update_t* manager) {
    if (!manager->subscribed_to_topic && manager->callback_count > 0) {
        if (MqttApp_Subscribe(SHARED_ATTRIBUTE_TOPIC, 0, shared_attribute_mqtt_callback, 5000)) {
            manager->subscribed_to_topic = true;
            printf("Subscribed to shared attribute topic\n");
            return true;
        } else {
            printf("Failed to subscribe to shared attribute topic\n");
            return false;
        }
    }
    return true;
}

bool shared_attribute_update_init(shared_attribute_update_t* manager) {
    if (!manager) {
        return false;
    }
    
    memset(manager, 0, sizeof(shared_attribute_update_t));
    manager->callback_count = 0;
    manager->subscribed_to_topic = false;
    g_shared_attr_manager = manager;
    
    return true;
}

bool shared_attribute_update_subscribe_single(shared_attribute_update_t* manager,
                                             const shared_attribute_callback_t* callback) {
    if (!manager || !callback) {
        return false;
    }
    
    if (manager->callback_count >= MAX_SHARED_ATTRIBUTE_UPDATE_SUBSCRIPTIONS) {
        printf("Maximum shared attribute update subscriptions reached\n");
        return false;
    }
    
    // Copy the callback to our internal storage
    memcpy(&manager->callbacks[manager->callback_count], callback, sizeof(shared_attribute_callback_t));
    manager->callbacks[manager->callback_count].active = true;
    manager->callback_count++;
    
    // Ensure we're subscribed to the MQTT topic
    if (!ensure_topic_subscription(manager)) {
        // Rollback the addition if subscription failed
        manager->callback_count--;
        return false;
    }
    
    printf("Added shared attribute update subscription (total: %zu)\n", manager->callback_count);
    return true;
}

bool shared_attribute_update_subscribe_multiple(shared_attribute_update_t* manager,
                                               const shared_attribute_callback_t* callbacks,
                                               size_t count) {
    if (!manager || !callbacks || count == 0) {
        return false;
    }
    
    if (manager->callback_count + count > MAX_SHARED_ATTRIBUTE_UPDATE_SUBSCRIPTIONS) {
        printf("Not enough space for %zu additional subscriptions\n", count);
        return false;
    }
    
    // Copy all callbacks to our internal storage
    for (size_t i = 0; i < count; i++) {
        memcpy(&manager->callbacks[manager->callback_count + i], &callbacks[i], sizeof(shared_attribute_callback_t));
        manager->callbacks[manager->callback_count + i].active = true;
    }
    manager->callback_count += count;
    
    // Ensure we're subscribed to the MQTT topic
    if (!ensure_topic_subscription(manager)) {
        // Rollback all additions if subscription failed
        manager->callback_count -= count;
        return false;
    }
    
    printf("Added %zu shared attribute update subscriptions (total: %zu)\n", count, manager->callback_count);
    return true;
}

bool shared_attribute_update_unsubscribe_all(shared_attribute_update_t* manager) {
    if (!manager) {
        return false;
    }
    
    // Clear all callbacks
    for (size_t i = 0; i < manager->callback_count; i++) {
        shared_attribute_callback_cleanup(&manager->callbacks[i]);
    }
    manager->callback_count = 0;
    
    // Unsubscribe from MQTT topic
    bool success = true;
    if (manager->subscribed_to_topic) {
        success = MqttApp_Unsubscribe(SHARED_ATTRIBUTE_TOPIC, 5000);
        manager->subscribed_to_topic = false;
        if (success) {
            printf("Unsubscribed from shared attribute topic\n");
        } else {
            printf("Failed to unsubscribe from shared attribute topic\n");
        }
    }
    
    return success;
}

void shared_attribute_update_process_message(shared_attribute_update_t* manager,
                                            const char* topic,
                                            const char* data,
                                            size_t data_len) {
    if (!manager || !topic || !data) {
        return;
    }
    
    // Check if this is a shared attribute update topic
    if (!shared_attribute_update_compare_topic(topic)) {
        return;
    }
    
    printf("Processing shared attribute update message\n");
    
    // Parse JSON to extract shared attributes
    struct mg_str json_str = mg_str_n(data, data_len);
    struct mg_str shared_data = {0};
    
    // Try to extract "shared" key from JSON
    int token_len;
    int offset = mg_json_get(json_str, "$.shared", &token_len);
    if (offset >= 0) {
        shared_data = mg_str_n(data + offset, token_len);
    } else {
        // If no "shared" key, use the whole data
        shared_data = json_str;
    }
    
    // Process each callback
    for (size_t i = 0; i < manager->callback_count; i++) {
        shared_attribute_callback_t* callback = &manager->callbacks[i];
        
        if (!callback->active || !callback->callback) {
            continue;
        }
        
        // If no specific attributes are monitored, call callback for any update
        if (shared_attribute_callback_is_empty(callback)) {
            callback->callback(shared_data.buf, shared_data.len);
            continue;
        }
        
        // Check if any monitored attributes are present in the update
        bool has_monitored_attribute = false;
        for (size_t j = 0; j < callback->attribute_count; j++) {
            char path[128];
            snprintf(path, sizeof(path), "$.%s", callback->attributes[j]);
            
            int attr_offset = mg_json_get(shared_data, path, &token_len);
            if (attr_offset >= 0) {
                has_monitored_attribute = true;
                break;
            }
        }
        
        if (has_monitored_attribute) {
            callback->callback(shared_data.buf, shared_data.len);
        }
    }
}

bool shared_attribute_update_compare_topic(const char* topic) {
    if (!topic) {
        return false;
    }
    
    return strncmp(SHARED_ATTRIBUTE_TOPIC, topic, strlen(SHARED_ATTRIBUTE_TOPIC)) == 0;
}

bool shared_attribute_update_resubscribe(shared_attribute_update_t* manager) {
    if (!manager) {
        return false;
    }
    
    if (manager->callback_count > 0 && !manager->subscribed_to_topic) {
        return ensure_topic_subscription(manager);
    }
    
    return true;
}

size_t shared_attribute_update_get_subscription_count(const shared_attribute_update_t* manager) {
    if (!manager) {
        return 0;
    }
    
    return manager->callback_count;
}

bool shared_attribute_update_has_subscriptions(const shared_attribute_update_t* manager) {
    if (!manager) {
        return false;
    }
    
    return manager->callback_count > 0;
}

void shared_attribute_update_cleanup(shared_attribute_update_t* manager) {
    if (!manager) {
        return;
    }
    
    shared_attribute_update_unsubscribe_all(manager);
    
    if (g_shared_attr_manager == manager) {
        g_shared_attr_manager = NULL;
    }
    
    memset(manager, 0, sizeof(shared_attribute_update_t));
}
