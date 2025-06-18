#ifndef RPC_CALLBACK_H
#define RPC_CALLBACK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RPC_METHOD_NAME_LENGTH 64
#define DEFAULT_RPC_RESPONSE_SIZE 256

// Forward declaration
struct mg_str;

/// @brief Server-side RPC callback function type
/// @param request_data JSON data containing the RPC request
/// @param request_len Length of the request data
/// @param response_data Buffer to write the response data
/// @param response_max_len Maximum length of the response buffer
/// @return Length of response data written, 0 if no response
typedef size_t (*rpc_callback_fn_t)(const char* request_data, size_t request_len, 
                                   char* response_data, size_t response_max_len);

/// @brief Server-side RPC callback wrapper structure,
/// contains the needed configuration settings to handle server-side RPC requests.
/// Documentation about the specific use of Server-side RPC in ThingsBoard can be found here:
/// https://thingsboard.io/docs/user-guide/rpc/#server-side-rpc
typedef struct {
    char method_name[MAX_RPC_METHOD_NAME_LENGTH];
    rpc_callback_fn_t callback;
    size_t response_size;
    bool active;
} rpc_callback_t;

// Function declarations

/**
 * @brief   Initialize RPC callback with empty values.
 * @param   [in] rpc_callback - Pointer to the RPC callback object to initialize.
 * @return  true - if successful, otherwise false
 */
bool rpc_callback_init(rpc_callback_t* rpc_callback);

/**
 * @brief   Create RPC callback with method name and callback function.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] method_name - Name of the RPC method to handle.
 * @param   [in] callback - Function to call when RPC request arrives.
 * @param   [in] response_size - Maximum size for response data.
 * @return  true - if successful, otherwise false
 */
bool rpc_callback_create(rpc_callback_t* rpc_callback, 
                        const char* method_name, 
                        rpc_callback_fn_t callback,
                        size_t response_size);

/**
 * @brief   Get the method name from RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @return  Pointer to method name - if successful, otherwise NULL
 */
const char* rpc_callback_get_name(const rpc_callback_t* rpc_callback);

/**
 * @brief   Set the method name for RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] method_name - Name of the RPC method to handle.
 * @return  true - if successful, otherwise false
 */
bool rpc_callback_set_name(rpc_callback_t* rpc_callback, const char* method_name);

/**
 * @brief   Get the response size from RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @return  Response size in bytes, 0 if error
 */
size_t rpc_callback_get_response_size(const rpc_callback_t* rpc_callback);

/**
 * @brief   Set the response size for RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] response_size - Maximum size for response data.
 * @return  true - if successful, otherwise false
 */
bool rpc_callback_set_response_size(rpc_callback_t* rpc_callback, size_t response_size);

/**
 * @brief   Get the callback function from RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @return  Pointer to callback function - if successful, otherwise NULL
 */
rpc_callback_fn_t rpc_callback_get_function(const rpc_callback_t* rpc_callback);

/**
 * @brief   Set the callback function for RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] callback - Function to call when RPC request arrives.
 * @return  true - if successful, otherwise false
 */
bool rpc_callback_set_function(rpc_callback_t* rpc_callback, rpc_callback_fn_t callback);

/**
 * @brief   Check if RPC callback is active and ready to handle requests.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @return  true - if active, otherwise false
 */
bool rpc_callback_is_active(const rpc_callback_t* rpc_callback);

/**
 * @brief   Check if method name matches the RPC callback.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] method_name - Method name to compare.
 * @return  true - if method name matches, otherwise false
 */
bool rpc_callback_matches_method(const rpc_callback_t* rpc_callback, const char* method_name);

/**
 * @brief   Call the RPC callback function with request data.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @param   [in] request_data - JSON data containing the RPC request.
 * @param   [in] request_len - Length of the request data.
 * @param   [out] response_data - Buffer to write the response data.
 * @param   [in] response_max_len - Maximum length of the response buffer.
 * @return  Length of response data written, 0 if no response or error
 */
size_t rpc_callback_call(const rpc_callback_t* rpc_callback,
                        const char* request_data, 
                        size_t request_len,
                        char* response_data, 
                        size_t response_max_len);

/**
 * @brief   Cleanup and reset the RPC callback object.
 * @param   [in] rpc_callback - Pointer to the RPC callback object.
 * @return  None
 */
void rpc_callback_cleanup(rpc_callback_t* rpc_callback);

#ifdef __cplusplus
}
#endif

#endif // RPC_CALLBACK_H
