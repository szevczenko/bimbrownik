/**
 *******************************************************************************
 * @file    app_events.c
 * @author  Dmytro Shevchenko
 * @brief   App events implementation
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "app_events.h"

#include <freertos/FreeRTOS.h>
#include <stddef.h>
#include <stdlib.h>

#include "app_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[AppEvent] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_APP_EVENT
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Private variables ---------------------------------------------------------*/
static uint32_t events_counter;
static const char* msg_id_name[] =
  {
#define MSG( _id ) [MSG_ID_##_id] = #_id,
    MSG_IDS_LIST
#undef MSG
};

static const char* event_task_name[] =
  {
#define EVENT_TASK( _id ) [APP_EVENT_##_id] = #_id,
    EVENTS_TASK_LIST
#undef EVENT_TASK
};

/* Public functions ----------------------------------------------------------*/

bool AppEventPrepareNoData( app_event_t* event, app_msg_id_t msg_id, app_events_task_t src, app_events_task_t dst )
{
  if ( event == NULL || src >= APP_EVENT_LAST )
  {
    LOG( PRINT_ERROR, "%s() Bad input arguments", __func__ );
    assert( 0 );
    return false;
  }

  event->msg_id = msg_id;
  event->src = src;
  event->dst = dst;
  event->event_number = events_counter++;

  event->data = NULL;
  event->data_size = 0;
  return true;
}

bool AppEventPrepareWithData( app_event_t* event, app_msg_id_t msg_id, app_events_task_t src, app_events_task_t dst, const void* data, uint32_t data_size )
{
  if ( event == NULL || data == NULL || data_size == 0 || src >= APP_EVENT_LAST )
  {
    LOG( PRINT_ERROR, "%s() Bad input arguments", __func__ );
    assert( 0 );
    return false;
  }
  void* m_data = malloc( data_size );
  if ( m_data == NULL )
  {
    LOG( PRINT_ERROR, "%s() Cannot allocate data %d", __func__, data_size );
    assert( 0 );
    return false;
  }
  memcpy( m_data, data, data_size );

  event->data_size = data_size;
  event->data = m_data;
  event->msg_id = msg_id;
  event->src = src;
  event->dst = dst;
  event->event_number = events_counter++;

  return true;
}

bool AppEventGetData( const app_event_t* event, void* data, uint32_t data_size )
{
  if ( event == NULL || event->data == NULL || event->data_size == 0 || data == NULL || data_size != event->data_size )
  {
    assert( 0 );
    return false;
  }

  memcpy( data, event->data, data_size );

  return true;
}

void AppEventDelete( app_event_t* event )
{
  if ( 0 < event->data_size )
  {
    if ( NULL != event->data )
    {
      free( event->data );
      event->data = NULL;
    }
    event->data_size = 0;
  }
  event->src = 0;
}

bool AppEventSearchAndExecute( const app_event_t* event, const struct app_events_handler* handlers, const uint8_t handlers_length )
{
  if ( ( event == NULL ) || ( handlers == NULL ) || ( handlers_length == 0 ) )
  {
    assert( 0 );
    return false;
  }

  bool found_id = false;

  for ( uint8_t i = 0; i < handlers_length; i++ )
  {
    if ( handlers[i].id == event->msg_id )
    {
      found_id = true;

      if ( handlers[i].callback != NULL )
      {
        LOG( PRINT_INFO, "%s -> %s: %s", event_task_name[event->src], event_task_name[event->dst], msg_id_name[event->msg_id] );
        handlers[i].callback( event );
        return true;
      }
      else
      {
        LOG( PRINT_ERROR, "%s: callback is NULL", __func__ );
        assert( 0 );
      }

      break;
    }
  }

  if (found_id == false)
  {
    LOG( PRINT_INFO, "Could not run %s: %s", event_task_name[event->dst], msg_id_name[event->msg_id] );
  }

  return found_id;
}
