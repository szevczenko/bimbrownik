/**
 *******************************************************************************
 * @file    app_timers.h
 * @author  Dmytro Shevchenko
 * @brief   Application timers implementation header
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/
#ifndef __APP_TIMERS_H__
#define __APP_TIMERS_H__

#include <stdbool.h>
#include <stdint.h>

#include "app_msg_id.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* Public macro --------------------------------------------------------------*/
#define TIMER_ITEM( _id, _callback, _timeout, _name ) \
  [_id] = { .callback = ( _callback ), .timeout_ms = ( _timeout ), .timer_name = ( _name ) }

/* Public types --------------------------------------------------------------*/
typedef struct
{
  TimerHandle_t timer;
  const char* timer_name;
  uint32_t timeout_ms;
  void ( *callback )( TimerHandle_t xTimer );
} app_timer_t;

/* Public functions ----------------------------------------------------------*/
/**
 * @brief   Init module timers.
 * @param   [in] timers - timers array.
 * @param   [in] timers_cnt - timers array size.
 */
void AppTimersInit( app_timer_t* timers, uint32_t timers_cnt );

/**
 * @brief   Start timer.
 * @param   [in] timers - timers array.
 * @param   [in] timers_cnt - timers array size.
 */
void AppTimerStart( app_timer_t* timers, uint32_t id );

/**
 * @brief   Stop timer.
 * @param   [in] timers - timers array.
 * @param   [in] timers_cnt - timers array size.
 */
void AppTimerStop( app_timer_t* timers, uint32_t id );

#endif /* __APP_TIMERS_H__ */
