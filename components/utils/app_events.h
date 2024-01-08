/**
 *******************************************************************************
 * @file    app_events.h
 * @author  Dmytro Shevchenko
 * @brief   Application events implementation header
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/
#ifndef __APP_EVENTS_H__
#define __APP_EVENTS_H__

#include <stdbool.h>
#include <stdint.h>

#include "app_msg_id.h"

#define EVENTS_TASK_LIST        \
  EVENT_TASK( APP_MANAGER )     \
  EVENT_TASK( NETWORK_MANAGER ) \
  EVENT_TASK( WIFI_DRV )        \
  EVENT_TASK( TCP_SERVER )      \
  EVENT_TASK( OTA )             \
  EVENT_TASK( MQTT_APP )        \
  EVENT_TASK( DEV_MANAGER )     \
  EVENT_TASK( TEMP_DRV )

/* Public macro --------------------------------------------------------------*/
#define ARRAY_SIZE( _array ) ( sizeof( _array ) / sizeof( ( _array )[0] ) )

#define EVENT_ITEM( _id, _callback )         \
  {                                          \
    .id = ( _id ), .callback = ( _callback ) \
  }

/* Public types --------------------------------------------------------------*/

typedef enum
{
#define EVENT_TASK( _event_task ) APP_EVENT_##_event_task,
  EVENTS_TASK_LIST
#undef EVENT_TASK
    APP_EVENT_LAST
} app_events_task_t;

typedef struct
{
  app_events_task_t src;
  app_events_task_t dst;
  app_msg_id_t msg_id;
  uint32_t event_number;
  uint32_t data_size;
  void* data;
} app_event_t;

typedef void ( *event_callback_t )( const app_event_t* );

struct app_events_handler
{
  const uint32_t id;
  const event_callback_t callback;
};

/* Public functions ----------------------------------------------------------*/
/**
 * @brief   Prepares event without data before send to queue.
 * @param   [in] event - Preparing event.
 * @param   [in] msg_id - Module message id.
 * @param   [in] src - Source module id.
 * @param   [in] dst - Destination module id.
 * @return  true - if event ready to send, otherwise false
 */
bool AppEventPrepareNoData( app_event_t* event, app_msg_id_t msg_id, app_events_task_t src, app_events_task_t dst );

/**
 * @brief   Prepares event with data before send to queue.
 * @param   [in] event - Preparing event.
 * @param   [in] msg_id - Module message id.
 * @param   [in] src - Source module id.
 * @param   [in] dst - Destination module id.
 * @param   [in] data - Data to send.
 * @param   [in] data_size - Data size for malloc memory.
 * @return  true - if event ready to send, otherwise false
 */
bool AppEventPrepareWithData( app_event_t* event, app_msg_id_t msg_id, app_events_task_t src, app_events_task_t dst, const void* data, uint32_t data_size );

/**
 * @brief   Get data from event.
 * @param   [in] event - Event.
 * @param   [in] data - Gets data.
 * @param   [in] data_size - Gets data size.
 * @return  true - if successful gets data, otherwise false
 */
bool AppEventGetData( const app_event_t* event, void* data, uint32_t data_size );

/**
 * @brief   Frees allocated memory in event.
 * @param   [in] event - Event.
 */
void AppEventDelete( app_event_t* event );

/**
 * @brief   Looks for specific event handler in given table, then executes it.
 * @param   [in] id - event id number.
 * @param   [in] handlers - table of event handlers.
 * @param   [in] handlers_length - length of the handler table.
 * @param   [in] data - pointer to event.
 * @return  true - if successful execute, otherwise false
 */
bool AppEventSearchAndExecute( const app_event_t* event, const struct app_events_handler* handlers, const uint8_t handlers_length );

#endif /* __APP_EVENTS_H__ */
