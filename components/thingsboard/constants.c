#include "constants.h"

// Log message string constants
const char TOO_MANY_JSON_FIELDS[] = "Attempt to enter too many JSON fields into buffer (%u), increase (%s) (%u) accordingly";
const char UNABLE_TO_SERIALIZE[] = "Unable to serialize key-value json";
const char CONNECT_FAILED[] = "Connecting to server failed";
const char UNABLE_TO_SERIALIZE_JSON[] = "Unable to serialize json data";
const char UNABLE_TO_ALLOCATE_JSON[] = "Allocating memory for the JsonDocument failed, passed JsonDocument is NULL";
const char JSON_SIZE_TO_SMALL[] = "JsonDocument too small to store all values. Ensure every key value pair gets JSON_OBJECT_SIZE(1) capacity + size required by value / key that is inserted";
const char MAX_SUBSCRIPTIONS_EXCEEDED[] = "Maximum amount of subscriptions (%u) exceeded for (%s)";
const char MAX_SUBSCRIPTIONS_TEMPLATE_NAME[] = "subscriptions";
const char SUBSCRIBE_TOPIC_FAILED[] = "Subscribing to topic (%s) failed";
const char REQUEST_ID_NULL[] = "Request ID is NULL";

// ThingsBoard API topic patterns
const char TELEMETRY_TOPIC[] = "v1/devices/me/telemetry";
const char ATTRIBUTE_TOPIC[] = "v1/devices/me/attributes";
