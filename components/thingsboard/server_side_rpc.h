#ifndef SERVER_SIDE_RPC_H
#define SERVER_SIDE_RPC_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "rpc_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SERVER_SIDE_RPC_SUBSCRIPTIONS 10
#define MAX_RPC_RESPONSE_SIZE 512

// Server side RPC topics
extern const char RPC_SUBSCRIBE_TOPIC[];
extern const char RPC_REQUEST_TOPIC[];
extern const char RPC_SEND_RESPONSE_TOPIC[];

// JSON keys
extern const char RPC_METHOD_KEY[];
extern const char RPC_PARAMS_KEY[];

/// @brief Server-side RPC manager structure,
/// handles the internal implementation of the ThingsBoard server side RPC API.
/// See https://thingsboard.io/docs/user-guide/rpc/#server-side-rpc for more information
typedef struct {
    rpc_callback_t callbacks[MAX_SERVER_SIDE_RPC_SUBSCRIPTIONS];
    size_t callback_count;
    bool subscribed_to_topic;
} server_side_rpc_t;

// Function declarations

/**
 * @brief   Initialize server-side RPC manager.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  true - if initialization successful, otherwise false
 */
bool server_side_rpc_init(server_side_rpc_t* manager);

/**
 * @brief   Subscribe to server-side RPC with a single callback.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @param   [in] callback - RPC callback configuration to subscribe.
 * @return  true - if subscription successful, otherwise false
 */
bool server_side_rpc_subscribe_single(server_side_rpc_t* manager, const rpc_callback_t* callback);

/**
 * @brief   Subscribe to server-side RPC with multiple callbacks.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @param   [in] callbacks - Array of RPC callback configurations to subscribe.
 * @param   [in] count - Number of callbacks in the array.
 * @return  true - if subscription successful, otherwise false
 */
bool server_side_rpc_subscribe_multiple(server_side_rpc_t* manager,
                                       const rpc_callback_t* callbacks,
                                       size_t count);

/**
 * @brief   Unsubscribe from all server-side RPC callbacks.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  true - if unsubscription successful, otherwise false
 */
bool server_side_rpc_unsubscribe_all(server_side_rpc_t* manager);

/**
 * @brief   Process incoming server-side RPC request message.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @param   [in] topic - MQTT topic of the RPC request.
 * @param   [in] data - Request data payload.
 * @param   [in] data_len - Length of request data.
 * @return  None
 */
void server_side_rpc_process_request(server_side_rpc_t* manager,
                                    const char* topic,
                                    const char* data,
                                    size_t data_len);

/**
 * @brief   Check if topic matches server-side RPC request topic.
 * @param   [in] topic - MQTT topic to check.
 * @return  true - if topic matches, otherwise false
 */
bool server_side_rpc_compare_topic(const char* topic);

/**
 * @brief   Resubscribe to server-side RPC topic if needed.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  true - if resubscription successful, otherwise false
 */
bool server_side_rpc_resubscribe(server_side_rpc_t* manager);

/**
 * @brief   Get the number of active RPC subscriptions.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  Number of active subscriptions, 0 if error
 */
size_t server_side_rpc_get_subscription_count(const server_side_rpc_t* manager);

/**
 * @brief   Check if manager has any active RPC subscriptions.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  true - if there are active subscriptions, otherwise false
 */
bool server_side_rpc_has_subscriptions(const server_side_rpc_t* manager);

/**
 * @brief   Parse request ID from RPC request topic.
 * @param   [in] topic - MQTT topic containing request ID.
 * @return  Request ID, 0 if parsing failed
 */
uint32_t server_side_rpc_parse_request_id(const char* topic);

/**
 * @brief   Send RPC response back to server.
 * @param   [in] request_id - Request ID from the original request.
 * @param   [in] response_data - JSON response data.
 * @param   [in] response_len - Length of response data.
 * @return  true - if response sent successfully, otherwise false
 */
bool server_side_rpc_send_response(uint32_t request_id, const char* response_data, size_t response_len);

/**
 * @brief   Cleanup server-side RPC manager and free resources.
 * @param   [in] manager - Server-side RPC manager pointer.
 * @return  None
 */
void server_side_rpc_cleanup(server_side_rpc_t* manager);

#ifdef __cplusplus
}
#endif

#endif // SERVER_SIDE_RPC_H
