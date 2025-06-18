#ifndef CONSTANTS_H
#define CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Default configuration values
#define DEFAULT_ENDPOINTS_AMOUNT 7
#define DEFAULT_RESPONSE_AMOUNT 8
#define DEFAULT_SUBSCRIPTIONS_AMOUNT 1
#define DEFAULT_ATTRIBUTES_AMOUNT 1
#define DEFAULT_RPC_AMOUNT 0
#define DEFAULT_REQUEST_RPC_AMOUNT 2
#define DEFAULT_PAYLOAD_SIZE 64
#define DEFAULT_MAX_STACK_SIZE 1024
#define DEFAULT_BUFFERING_SIZE 64
#define DEFAULT_MAX_RESPONSE_SIZE 0

// JSON sizing macros
#define JSON_OBJECT_SIZE(n) (((n) * 16) + 16)
#define JSON_ARRAY_SIZE(n) (((n) * 8) + 8)
#define JSON_STRING_SIZE(len) ((len) + 16)

// Log message constants
extern const char TOO_MANY_JSON_FIELDS[];
extern const char UNABLE_TO_SERIALIZE[];
extern const char CONNECT_FAILED[];
extern const char UNABLE_TO_SERIALIZE_JSON[];
extern const char UNABLE_TO_ALLOCATE_JSON[];
extern const char JSON_SIZE_TO_SMALL[];
extern const char MAX_SUBSCRIPTIONS_EXCEEDED[];
extern const char MAX_SUBSCRIPTIONS_TEMPLATE_NAME[];
extern const char SUBSCRIBE_TOPIC_FAILED[];
extern const char REQUEST_ID_NULL[];

// ThingsBoard API topic patterns
extern const char TELEMETRY_TOPIC[];
extern const char ATTRIBUTE_TOPIC[];

// Common buffer sizes
#define MAX_TOPIC_SIZE 256
#define MAX_MESSAGE_SIZE 1024
#define MAX_JSON_SIZE 2048
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 256

// Timeout values (in milliseconds)
#define DEFAULT_TIMEOUT_MS 5000
#define KEEP_ALIVE_TIMEOUT_MS 60000
#define RECONNECT_TIMEOUT_MS 30000

// Quality of Service levels
#define QOS_AT_MOST_ONCE 0
#define QOS_AT_LEAST_ONCE 1
#define QOS_EXACTLY_ONCE 2

// Return codes
#define TB_SUCCESS 0
#define TB_ERROR -1
#define TB_TIMEOUT -2
#define TB_NO_MEMORY -3
#define TB_INVALID_PARAM -4

// Utility macros
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// String helper macros
#define STRING_IS_NULL_OR_EMPTY(str) ((str) == NULL || (str)[0] == '\0')
#define SAFE_STRING(str) ((str) ? (str) : "")

// Memory alignment
#define ALIGN_SIZE(size, align) (((size) + (align) - 1) & ~((align) - 1))

// Boolean helpers for C
#ifndef __cplusplus
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif // CONSTANTS_H
