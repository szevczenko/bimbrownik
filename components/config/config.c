#include "app_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* error_lvl_str[] =
  {
    [PRINT_DEBUG] = "DEBUG: ",
    [PRINT_INFO] = "INFO: ",
    [PRINT_WARNING] = "WARNING: ",
    [PRINT_ERROR] = "ERROR: " };

static SemaphoreHandle_t mutexSemaphore;

void config_printf( enum config_print_lvl module_lvl, enum config_print_lvl msg_lvl, const char* format, ... )
{
  if ( module_lvl <= msg_lvl )
  {
    xSemaphoreTake( mutexSemaphore, 250 );
    printf( error_lvl_str[msg_lvl] );
    va_list args;
    va_start( args, format );
    vprintf( format, args );
    va_end( args );
    printf( "\n\r" );
    xSemaphoreGive( mutexSemaphore );
  }
}

void configInit( void )
{
  mutexSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive( mutexSemaphore );
}
