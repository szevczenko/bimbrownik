#ifndef RPC_REQUEST_CALLBACK_H
#define RPC_REQUEST_CALLBACK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RPC_METHOD_NAME_LENGTH 64
#define MAX_RPC_PARAMETERS_LENGTH 512

// Forward declaration
struct mg_str;

/// @brief Client-side RPC received response callback function type
/// @param response_data JSON data containing the RPC response
/// @param response_len Length of the response data
typedef void (*rpc_response_callback_fn_t)(const char* response_data, size_t response_len);

/// @brief Client-side RPC timeout callback function type
typedef void (*rpc_timeout_callback_fn_t)(void);

/// @brief Client-side RPC request callback wrapper structure,
/// contains the needed configuration settings to create and handle client-side RPC requests.
/// Documentation about the specific use of client-side RPC in ThingsBoard can be found here:
/// https://thingsboard.io/docs/user-guide/rpc/#client-side-rpc
typedef struct {
    char method_name[MAX_RPC_METHOD_NAME_LENGTH];
    char parameters[MAX_RPC_PARAMETERS_LENGTH];
    size_t request_id;
    uint64_t timeout_microseconds;
    uint64_t start_time_us;
    rpc_response_callback_fn_t received_callback;
    rpc_timeout_callback_fn_t timeout_callback;
    bool active;
    bool timeout_started;
} rpc_request_callback_t;

// Function declarations

/**
 * @brief   Initialize RPC request callback with empty values.
 * @param   [in] rpc_request - Pointer to the RPC request callback object to initialize.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_init(rpc_request_callback_t* rpc_request);

/**
 * @brief   Create RPC request callback with method name and callbacks.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] method_name - Name of the client-side RPC method to call on the cloud.
 * @param   [in] received_callback - Function to call when RPC response arrives.
 * @param   [in] parameters - Optional JSON parameters string for the method call.
 * @param   [in] timeout_microseconds - Timeout in microseconds, 0 for no timeout.
 * @param   [in] timeout_callback - Function to call on timeout, can be NULL.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_create(rpc_request_callback_t* rpc_request,
                                const char* method_name,
                                rpc_response_callback_fn_t received_callback,
                                const char* parameters,
                                uint64_t timeout_microseconds,
                                rpc_timeout_callback_fn_t timeout_callback);

/**
 * @brief   Get the unique request identifier.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  Request ID, 0 if error
 */
size_t rpc_request_callback_get_request_id(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Set the unique request identifier.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] request_id - Unique identifier for the request.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_request_id(rpc_request_callback_t* rpc_request, size_t request_id);

/**
 * @brief   Get the method name from RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  Pointer to method name - if successful, otherwise NULL
 */
const char* rpc_request_callback_get_name(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Set the method name for RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] method_name - Name of the RPC method to call.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_name(rpc_request_callback_t* rpc_request, const char* method_name);

/**
 * @brief   Get the parameters from RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  Pointer to parameters string - if successful, otherwise NULL
 */
const char* rpc_request_callback_get_parameters(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Set the parameters for RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] parameters - JSON parameters string for the method call.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_parameters(rpc_request_callback_t* rpc_request, const char* parameters);

/**
 * @brief   Get the timeout value from RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  Timeout in microseconds, 0 if error or no timeout
 */
uint64_t rpc_request_callback_get_timeout(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Set the timeout value for RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] timeout_microseconds - Timeout in microseconds, 0 for no timeout.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_timeout(rpc_request_callback_t* rpc_request, uint64_t timeout_microseconds);

/**
 * @brief   Get the received callback function from RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  Pointer to callback function - if successful, otherwise NULL
 */
rpc_response_callback_fn_t rpc_request_callback_get_received_function(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Set the received callback function for RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] received_callback - Function to call when response arrives.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_received_function(rpc_request_callback_t* rpc_request, 
                                               rpc_response_callback_fn_t received_callback);

/**
 * @brief   Set the timeout callback function for RPC request callback.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] timeout_callback - Function to call on timeout, can be NULL.
 * @return  true - if successful, otherwise false
 */
bool rpc_request_callback_set_timeout_function(rpc_request_callback_t* rpc_request, 
                                              rpc_timeout_callback_fn_t timeout_callback);

/**
 * @brief   Check if RPC request callback is active.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  true - if active, otherwise false
 */
bool rpc_request_callback_is_active(const rpc_request_callback_t* rpc_request);

/**
 * @brief   Start the timeout timer for the RPC request.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] current_time_us - Current time in microseconds.
 * @return  true - if timer started or no timeout configured, otherwise false
 */
bool rpc_request_callback_start_timeout_timer(rpc_request_callback_t* rpc_request, uint64_t current_time_us);

/**
 * @brief   Stop the timeout timer for the RPC request.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  None
 */
void rpc_request_callback_stop_timeout_timer(rpc_request_callback_t* rpc_request);

/**
 * @brief   Update and check timeout timer for the RPC request.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] current_time_us - Current time in microseconds.
 * @return  true - if timeout occurred and callback was called, otherwise false
 */
bool rpc_request_callback_update_timeout_timer(rpc_request_callback_t* rpc_request, uint64_t current_time_us);

/**
 * @brief   Call the received callback function with response data.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @param   [in] response_data - JSON data containing the RPC response.
 * @param   [in] response_len - Length of the response data.
 * @return  true - if callback was called, otherwise false
 */
bool rpc_request_callback_call_received(const rpc_request_callback_t* rpc_request,
                                       const char* response_data,
                                       size_t response_len);

/**
 * @brief   Cleanup and reset the RPC request callback object.
 * @param   [in] rpc_request - Pointer to the RPC request callback object.
 * @return  None
 */
void rpc_request_callback_cleanup(rpc_request_callback_t* rpc_request);

#ifdef __cplusplus
}
#endif

#endif // RPC_REQUEST_CALLBACK_H
