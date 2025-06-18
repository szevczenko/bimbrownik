#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "mongoose.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ATTRIBUTE_SUBSCRIPTIONS 5
#define MAX_ATTRIBUTES_PER_REQUEST 10
#define MAX_TOPIC_LENGTH 128
#define MAX_ATTRIBUTE_KEY_LENGTH 64

// MQTT topic templates
extern const char ATTRIBUTE_REQUEST_TOPIC[];
extern const char ATTRIBUTE_RESPONSE_SUBSCRIBE_TOPIC[];
extern const char ATTRIBUTE_RESPONSE_TOPIC[];

// Request/response keys
extern const char CLIENT_REQUEST_KEYS[];
extern const char CLIENT_RESPONSE_KEY[];
extern const char SHARED_REQUEST_KEY[];
extern const char SHARED_RESPONSE_KEY[];

typedef enum {
    ATTRIBUTE_TYPE_CLIENT,
    ATTRIBUTE_TYPE_SHARED
} attribute_type_t;

typedef struct {
    char keys[MAX_ATTRIBUTES_PER_REQUEST][MAX_ATTRIBUTE_KEY_LENGTH];
    size_t count;
    uint32_t request_id;
    uint32_t timeout_ms;
    uint32_t start_time;
    attribute_type_t type;
    void (*callback)(const char* topic, const char* data, size_t data_len);
    bool active;
} attribute_request_t;

/// @brief Attributes manager structure for handling attribute requests and responses
typedef struct {
    attribute_request_t requests[MAX_ATTRIBUTE_SUBSCRIPTIONS];
    size_t active_count;
    bool subscribed_to_responses;
    bool subscribed_to_topic;
    uint32_t next_request_id;
} attributes_manager_t;

// Function declarations

/**
 * @brief   Initialize attributes manager.
 * @param   [in] manager - Attributes manager pointer.
 * @return  true - if initialization successful, otherwise false
 */
bool attributes_init(attributes_manager_t* manager);

/**
 * @brief   Request client-side attributes from server.
 * @param   [in] manager - Attributes manager pointer.
 * @param   [in] attribute_keys - Array of attribute key names.
 * @param   [in] keys_count - Number of keys in the array.
 * @param   [in] timeout_ms - Request timeout in milliseconds.
 * @param   [in] callback - Function to call when response arrives.
 * @return  true - if request sent successfully, otherwise false
 */
bool attributes_request_client(attributes_manager_t* manager,
                              const char* const* attribute_keys,
                              size_t keys_count,
                              uint32_t timeout_ms,
                              void (*callback)(const char* topic, const char* data, size_t data_len));

/**
 * @brief   Request shared attributes from server.
 * @param   [in] manager - Attributes manager pointer.
 * @param   [in] attribute_keys - Array of attribute key names.
 * @param   [in] keys_count - Number of keys in the array.
 * @param   [in] timeout_ms - Request timeout in milliseconds.
 * @param   [in] callback - Function to call when response arrives.
 * @return  true - if request sent successfully, otherwise false
 */
bool attributes_request_shared(attributes_manager_t* manager,
                              const char* const* attribute_keys,
                              size_t keys_count,
                              uint32_t timeout_ms,
                              void (*callback)(const char* topic, const char* data, size_t data_len));

/**
 * @brief   Process incoming attribute response message.
 * @param   [in] manager - Attributes manager pointer.
 * @param   [in] topic - MQTT topic of the response.
 * @param   [in] data - Response data payload.
 * @param   [in] data_len - Length of response data.
 * @return  None
 */
void attributes_process_response(attributes_manager_t* manager, const char* topic, const char* data, size_t data_len);

/**
 * @brief   Update timeouts for pending requests.
 * @param   [in] manager - Attributes manager pointer.
 * @param   [in] current_time_ms - Current system time in milliseconds.
 * @return  None
 */
void attributes_update_timeouts(attributes_manager_t* manager, uint32_t current_time_ms);

/**
 * @brief   Unsubscribe from all attribute requests.
 * @param   [in] manager - Attributes manager pointer.
 * @return  true - if unsubscription successful, otherwise false
 */
bool attributes_unsubscribe_all(attributes_manager_t* manager);

/**
 * @brief   Cleanup attributes manager and free resources.
 * @param   [in] manager - Attributes manager pointer.
 * @return  None
 */
void attributes_cleanup(attributes_manager_t* manager);

#ifdef __cplusplus
}
#endif

#endif // ATTRIBUTES_H
