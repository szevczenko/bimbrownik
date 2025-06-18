#ifndef SHARED_ATTRIBUTE_CALLBACK_H
#define SHARED_ATTRIBUTE_CALLBACK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SHARED_ATTRIBUTES     10
#define MAX_ATTRIBUTE_NAME_LENGTH 64

// Forward declaration
struct mg_str;

/// @brief Shared attribute update callback function type
/// @param data JSON data containing the updated attributes
/// @param data_len Length of the JSON data
typedef void ( *shared_attribute_callback_fn_t )( const char* data, size_t data_len );

/// @brief Shared attribute callback wrapper structure,
/// contains the needed configuration settings to subscribe to shared attribute updates.
/// Documentation about shared attribute updates in ThingsBoard can be found here:
/// https://thingsboard.io/docs/reference/mqtt-api/#subscribe-to-attribute-updates-from-the-server
typedef struct
{
  char attributes[MAX_SHARED_ATTRIBUTES][MAX_ATTRIBUTE_NAME_LENGTH];
  size_t attribute_count;
  shared_attribute_callback_fn_t callback;
  bool active;
} shared_attribute_callback_t;

// Function declarations

/**
 * @brief   Initialize a shared attribute callback with empty attributes.
 * @param   [in] callback_obj - Pointer to the callback object to initialize.
 * @return  true - if successful, otherwise false
 */
bool shared_attribute_callback_init(shared_attribute_callback_t* callback_obj);

/**
 * @brief   Set the callback function that will be called when attributes are updated.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [in] callback - Function to call when attributes are updated.
 * @return  true - if successful, otherwise false
 */
bool shared_attribute_callback_set_function(shared_attribute_callback_t* callback_obj, 
                                           shared_attribute_callback_fn_t callback);

/**
 * @brief   Add a single attribute to monitor for updates.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [in] attribute_name - Name of the attribute to monitor.
 * @return  true - if successful, otherwise false if no space available or invalid parameters
 */
bool shared_attribute_callback_add_attribute(shared_attribute_callback_t* callback_obj, 
                                            const char* attribute_name);

/**
 * @brief   Set multiple attributes to monitor for updates (replaces existing list).
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [in] attribute_names - Array of attribute name pointers.
 * @param   [in] count - Number of attributes in the array.
 * @return  true - if successful, otherwise false
 */
bool shared_attribute_callback_set_attributes(shared_attribute_callback_t* callback_obj,
                                             const char* const* attribute_names,
                                             size_t count);

/**
 * @brief   Get the list of attributes being monitored.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [out] attributes - Output array to store attribute name pointers.
 * @param   [in] max_count - Maximum number of attributes that can be stored.
 * @return  Number of attributes returned, 0 if error
 */
size_t shared_attribute_callback_get_attributes(const shared_attribute_callback_t* callback_obj,
                                               const char** attributes,
                                               size_t max_count);

/**
 * @brief   Get the number of attributes being monitored.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @return  Number of attributes, 0 if error
 */
size_t shared_attribute_callback_get_count(const shared_attribute_callback_t* callback_obj);

/**
 * @brief   Check if a specific attribute is being monitored.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [in] attribute_name - Name of the attribute to check.
 * @return  true - if the attribute is being monitored, otherwise false
 */
bool shared_attribute_callback_has_attribute(const shared_attribute_callback_t* callback_obj,
                                            const char* attribute_name);

/**
 * @brief   Clear all monitored attributes.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @return  None
 */
void shared_attribute_callback_clear_attributes(shared_attribute_callback_t* callback_obj);

/**
 * @brief   Check if the callback has any attributes to monitor.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @return  true - if no attributes are set, otherwise false
 */
bool shared_attribute_callback_is_empty(const shared_attribute_callback_t* callback_obj);

/**
 * @brief   Process received JSON data and call callback if any monitored attributes are present.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @param   [in] json_data - JSON string containing attribute updates.
 * @param   [in] data_len - Length of the JSON data.
 * @return  true - if callback was called, otherwise false
 */
bool shared_attribute_callback_process_data(const shared_attribute_callback_t* callback_obj,
                                           const char* json_data,
                                           size_t data_len);

/**
 * @brief   Cleanup and reset the callback object.
 * @param   [in] callback_obj - Pointer to the callback object.
 * @return  None
 */
void shared_attribute_callback_cleanup(shared_attribute_callback_t* callback_obj );

#ifdef __cplusplus
}
#endif

#endif    // SHARED_ATTRIBUTE_CALLBACK_H
