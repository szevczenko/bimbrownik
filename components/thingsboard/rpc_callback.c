#include "rpc_callback.h"
#include <string.h>
#include <stdio.h>

bool rpc_callback_init(rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return false;
    }
    
    memset(rpc_callback, 0, sizeof(rpc_callback_t));
    rpc_callback->response_size = DEFAULT_RPC_RESPONSE_SIZE;
    rpc_callback->active = false;
    
    return true;
}

bool rpc_callback_create(rpc_callback_t* rpc_callback, 
                        const char* method_name, 
                        rpc_callback_fn_t callback,
                        size_t response_size) {
    if (!rpc_callback || !method_name || !callback) {
        return false;
    }
    
    if (strlen(method_name) >= MAX_RPC_METHOD_NAME_LENGTH) {
        printf("RPC method name too long: %s\n", method_name);
        return false;
    }
    
    // Initialize the callback
    if (!rpc_callback_init(rpc_callback)) {
        return false;
    }
    
    // Set the values
    strncpy(rpc_callback->method_name, method_name, MAX_RPC_METHOD_NAME_LENGTH - 1);
    rpc_callback->method_name[MAX_RPC_METHOD_NAME_LENGTH - 1] = '\0';
    rpc_callback->callback = callback;
    rpc_callback->response_size = response_size > 0 ? response_size : DEFAULT_RPC_RESPONSE_SIZE;
    rpc_callback->active = true;
    
    return true;
}

const char* rpc_callback_get_name(const rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return NULL;
    }
    
    return rpc_callback->method_name;
}

bool rpc_callback_set_name(rpc_callback_t* rpc_callback, const char* method_name) {
    if (!rpc_callback || !method_name) {
        return false;
    }
    
    if (strlen(method_name) >= MAX_RPC_METHOD_NAME_LENGTH) {
        printf("RPC method name too long: %s\n", method_name);
        return false;
    }
    
    strncpy(rpc_callback->method_name, method_name, MAX_RPC_METHOD_NAME_LENGTH - 1);
    rpc_callback->method_name[MAX_RPC_METHOD_NAME_LENGTH - 1] = '\0';
    
    return true;
}

size_t rpc_callback_get_response_size(const rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return 0;
    }
    
    return rpc_callback->response_size;
}

bool rpc_callback_set_response_size(rpc_callback_t* rpc_callback, size_t response_size) {
    if (!rpc_callback) {
        return false;
    }
    
    rpc_callback->response_size = response_size > 0 ? response_size : DEFAULT_RPC_RESPONSE_SIZE;
    
    return true;
}

rpc_callback_fn_t rpc_callback_get_function(const rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return NULL;
    }
    
    return rpc_callback->callback;
}

bool rpc_callback_set_function(rpc_callback_t* rpc_callback, rpc_callback_fn_t callback) {
    if (!rpc_callback || !callback) {
        return false;
    }
    
    rpc_callback->callback = callback;
    rpc_callback->active = true;
    
    return true;
}

bool rpc_callback_is_active(const rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return false;
    }
    
    return rpc_callback->active && rpc_callback->callback != NULL;
}

bool rpc_callback_matches_method(const rpc_callback_t* rpc_callback, const char* method_name) {
    if (!rpc_callback || !method_name) {
        return false;
    }
    
    if (!rpc_callback->active) {
        return false;
    }
    
    return strcmp(rpc_callback->method_name, method_name) == 0;
}

size_t rpc_callback_call(const rpc_callback_t* rpc_callback,
                        const char* request_data, 
                        size_t request_len,
                        char* response_data, 
                        size_t response_max_len) {
    if (!rpc_callback || !request_data || !response_data) {
        return 0;
    }
    
    if (!rpc_callback->active || !rpc_callback->callback) {
        return 0;
    }
    
    // Ensure response buffer has enough space
    if (response_max_len < rpc_callback->response_size) {
        printf("Response buffer too small: %zu < %zu\n", response_max_len, rpc_callback->response_size);
        return 0;
    }
    
    // Call the user callback function
    return rpc_callback->callback(request_data, request_len, response_data, response_max_len);
}

void rpc_callback_cleanup(rpc_callback_t* rpc_callback) {
    if (!rpc_callback) {
        return;
    }
    
    memset(rpc_callback, 0, sizeof(rpc_callback_t));
    rpc_callback->response_size = DEFAULT_RPC_RESPONSE_SIZE;
    rpc_callback->active = false;
}
