/**
 *******************************************************************************
 * @file    app_timers.c
 * @author  Dmytro Shevchenko
 * @brief   App timers implementation
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "app_timers.h"

#include <freertos/FreeRTOS.h>
#include <stddef.h>
#include <stdlib.h>

#include "app_config.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[AppTimer] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_APP_EVENT
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

/* Public functions ----------------------------------------------------------*/

void AppTimersInit( app_timer_t* timers, uint32_t timers_cnt )
{
  for ( int i = 0; i < timers_cnt; i++ )
  {
    timers[i].timer = xTimerCreate( timers[i].timer_name, pdMS_TO_TICKS( timers[i].timeout_ms ), pdFALSE, NULL, timers[i].callback );
  }
}

void AppTimerStart( app_timer_t* timers, uint32_t id )
{
  if ( xTimerStart( timers[id].timer, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}

void AppTimerStop( app_timer_t* timers, uint32_t id )
{
  if ( xTimerStop( timers[id].timer, 0 ) != pdPASS )
  {
    assert( 0 );
  }
}
