#include "rpc_request_callback.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Helper function to get current time in microseconds
static uint64_t get_current_time_us(void) {
    return pdTICKS_TO_MS(xTaskGetTickCount()) * 1000;
}

bool rpc_request_callback_init(rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return false;
    }
    
    memset(rpc_request, 0, sizeof(rpc_request_callback_t));
    rpc_request->active = false;
    rpc_request->timeout_started = false;
    
    return true;
}

bool rpc_request_callback_create(rpc_request_callback_t* rpc_request,
                                const char* method_name,
                                rpc_response_callback_fn_t received_callback,
                                const char* parameters,
                                uint64_t timeout_microseconds,
                                rpc_timeout_callback_fn_t timeout_callback) {
    if (!rpc_request || !method_name || !received_callback) {
        return false;
    }
    
    if (strlen(method_name) >= MAX_RPC_METHOD_NAME_LENGTH) {
        printf("RPC method name too long: %s\n", method_name);
        return false;
    }
    
    // Initialize the callback
    if (!rpc_request_callback_init(rpc_request)) {
        return false;
    }
    
    // Set the values
    strncpy(rpc_request->method_name, method_name, MAX_RPC_METHOD_NAME_LENGTH - 1);
    rpc_request->method_name[MAX_RPC_METHOD_NAME_LENGTH - 1] = '\0';
    
    if (parameters) {
        if (strlen(parameters) >= MAX_RPC_PARAMETERS_LENGTH) {
            printf("RPC parameters too long\n");
            return false;
        }
        strncpy(rpc_request->parameters, parameters, MAX_RPC_PARAMETERS_LENGTH - 1);
        rpc_request->parameters[MAX_RPC_PARAMETERS_LENGTH - 1] = '\0';
    }
    
    rpc_request->received_callback = received_callback;
    rpc_request->timeout_microseconds = timeout_microseconds;
    rpc_request->timeout_callback = timeout_callback;
    rpc_request->active = true;
    
    return true;
}

size_t rpc_request_callback_get_request_id(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return 0;
    }
    
    return rpc_request->request_id;
}

bool rpc_request_callback_set_request_id(rpc_request_callback_t* rpc_request, size_t request_id) {
    if (!rpc_request) {
        return false;
    }
    
    rpc_request->request_id = request_id;
    
    return true;
}

const char* rpc_request_callback_get_name(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return NULL;
    }
    
    return rpc_request->method_name;
}

bool rpc_request_callback_set_name(rpc_request_callback_t* rpc_request, const char* method_name) {
    if (!rpc_request || !method_name) {
        return false;
    }
    
    if (strlen(method_name) >= MAX_RPC_METHOD_NAME_LENGTH) {
        printf("RPC method name too long: %s\n", method_name);
        return false;
    }
    
    strncpy(rpc_request->method_name, method_name, MAX_RPC_METHOD_NAME_LENGTH - 1);
    rpc_request->method_name[MAX_RPC_METHOD_NAME_LENGTH - 1] = '\0';
    
    return true;
}

const char* rpc_request_callback_get_parameters(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return NULL;
    }
    
    return strlen(rpc_request->parameters) > 0 ? rpc_request->parameters : NULL;
}

bool rpc_request_callback_set_parameters(rpc_request_callback_t* rpc_request, const char* parameters) {
    if (!rpc_request) {
        return false;
    }
    
    if (parameters) {
        if (strlen(parameters) >= MAX_RPC_PARAMETERS_LENGTH) {
            printf("RPC parameters too long\n");
            return false;
        }
        strncpy(rpc_request->parameters, parameters, MAX_RPC_PARAMETERS_LENGTH - 1);
        rpc_request->parameters[MAX_RPC_PARAMETERS_LENGTH - 1] = '\0';
    } else {
        rpc_request->parameters[0] = '\0';
    }
    
    return true;
}

uint64_t rpc_request_callback_get_timeout(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return 0;
    }
    
    return rpc_request->timeout_microseconds;
}

bool rpc_request_callback_set_timeout(rpc_request_callback_t* rpc_request, uint64_t timeout_microseconds) {
    if (!rpc_request) {
        return false;
    }
    
    rpc_request->timeout_microseconds = timeout_microseconds;
    
    return true;
}

rpc_response_callback_fn_t rpc_request_callback_get_received_function(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return NULL;
    }
    
    return rpc_request->received_callback;
}

bool rpc_request_callback_set_received_function(rpc_request_callback_t* rpc_request, 
                                               rpc_response_callback_fn_t received_callback) {
    if (!rpc_request || !received_callback) {
        return false;
    }
    
    rpc_request->received_callback = received_callback;
    rpc_request->active = true;
    
    return true;
}

bool rpc_request_callback_set_timeout_function(rpc_request_callback_t* rpc_request, 
                                              rpc_timeout_callback_fn_t timeout_callback) {
    if (!rpc_request) {
        return false;
    }
    
    rpc_request->timeout_callback = timeout_callback;
    
    return true;
}

bool rpc_request_callback_is_active(const rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return false;
    }
    
    return rpc_request->active && rpc_request->received_callback != NULL;
}

bool rpc_request_callback_start_timeout_timer(rpc_request_callback_t* rpc_request, uint64_t current_time_us) {
    if (!rpc_request) {
        return false;
    }
    
    if (rpc_request->timeout_microseconds == 0) {
        // No timeout configured
        return true;
    }
    
    rpc_request->start_time_us = current_time_us;
    rpc_request->timeout_started = true;
    
    return true;
}

void rpc_request_callback_stop_timeout_timer(rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return;
    }
    
    rpc_request->timeout_started = false;
}

bool rpc_request_callback_update_timeout_timer(rpc_request_callback_t* rpc_request, uint64_t current_time_us) {
    if (!rpc_request || !rpc_request->timeout_started) {
        return false;
    }
    
    if (rpc_request->timeout_microseconds == 0) {
        return false;
    }
    
    uint64_t elapsed_time = current_time_us - rpc_request->start_time_us;
    
    if (elapsed_time >= rpc_request->timeout_microseconds) {
        // Timeout occurred
        rpc_request->timeout_started = false;
        
        if (rpc_request->timeout_callback) {
            printf("RPC request timeout for method: %s (ID: %zu)\n", 
                   rpc_request->method_name, rpc_request->request_id);
            rpc_request->timeout_callback();
            return true;
        }
    }
    
    return false;
}

bool rpc_request_callback_call_received(const rpc_request_callback_t* rpc_request,
                                       const char* response_data,
                                       size_t response_len) {
    if (!rpc_request || !response_data || !rpc_request->received_callback) {
        return false;
    }
    
    if (!rpc_request->active) {
        return false;
    }
    
    rpc_request->received_callback(response_data, response_len);
    return true;
}

void rpc_request_callback_cleanup(rpc_request_callback_t* rpc_request) {
    if (!rpc_request) {
        return;
    }
    
    rpc_request_callback_stop_timeout_timer(rpc_request);
    memset(rpc_request, 0, sizeof(rpc_request_callback_t));
    rpc_request->active = false;
    rpc_request->timeout_started = false;
}
