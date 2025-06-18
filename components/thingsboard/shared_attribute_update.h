#ifndef SHARED_ATTRIBUTE_UPDATE_H
#define SHARED_ATTRIBUTE_UPDATE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "shared_attribute_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SHARED_ATTRIBUTE_UPDATE_SUBSCRIPTIONS 10
#define SHARED_ATTRIBUTE_TOPIC "v1/devices/me/attributes"

/// @brief Shared attribute update manager structure,
/// handles the internal implementation of the ThingsBoard shared attribute update API.
/// See https://thingsboard.io/docs/reference/mqtt-api/#subscribe-to-attribute-updates-from-the-server for more information
typedef struct {
    shared_attribute_callback_t callbacks[MAX_SHARED_ATTRIBUTE_UPDATE_SUBSCRIPTIONS];
    size_t callback_count;
    bool subscribed_to_topic;
} shared_attribute_update_t;

// Function declarations

/**
 * @brief   Initialize shared attribute update manager.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  true - if initialization successful, otherwise false
 */
bool shared_attribute_update_init(shared_attribute_update_t* manager);

/**
 * @brief   Subscribe to shared attribute updates with a single callback.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @param   [in] callback - Callback configuration to subscribe.
 * @return  true - if subscription successful, otherwise false
 */
bool shared_attribute_update_subscribe_single(shared_attribute_update_t* manager,
                                             const shared_attribute_callback_t* callback);

/**
 * @brief   Subscribe to shared attribute updates with multiple callbacks.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @param   [in] callbacks - Array of callback configurations to subscribe.
 * @param   [in] count - Number of callbacks in the array.
 * @return  true - if subscription successful, otherwise false
 */
bool shared_attribute_update_subscribe_multiple(shared_attribute_update_t* manager,
                                               const shared_attribute_callback_t* callbacks,
                                               size_t count);

/**
 * @brief   Unsubscribe from all shared attribute updates.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  true - if unsubscription successful, otherwise false
 */
bool shared_attribute_update_unsubscribe_all(shared_attribute_update_t* manager);

/**
 * @brief   Process incoming shared attribute update message.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @param   [in] topic - MQTT topic of the update.
 * @param   [in] data - Update data payload.
 * @param   [in] data_len - Length of update data.
 * @return  None
 */
void shared_attribute_update_process_message(shared_attribute_update_t* manager,
                                            const char* topic,
                                            const char* data,
                                            size_t data_len);

/**
 * @brief   Check if topic matches shared attribute update topic.
 * @param   [in] topic - MQTT topic to check.
 * @return  true - if topic matches, otherwise false
 */
bool shared_attribute_update_compare_topic(const char* topic);

/**
 * @brief   Resubscribe to shared attribute topic if needed.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  true - if resubscription successful, otherwise false
 */
bool shared_attribute_update_resubscribe(shared_attribute_update_t* manager);

/**
 * @brief   Get the number of active subscriptions.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  Number of active subscriptions, 0 if error
 */
size_t shared_attribute_update_get_subscription_count(const shared_attribute_update_t* manager);

/**
 * @brief   Check if manager has any active subscriptions.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  true - if there are active subscriptions, otherwise false
 */
bool shared_attribute_update_has_subscriptions(const shared_attribute_update_t* manager);

/**
 * @brief   Cleanup shared attribute update manager and free resources.
 * @param   [in] manager - Shared attribute update manager pointer.
 * @return  None
 */
void shared_attribute_update_cleanup(shared_attribute_update_t* manager);

#ifdef __cplusplus
}
#endif

#endif // SHARED_ATTRIBUTE_UPDATE_H
