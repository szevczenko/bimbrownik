#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "configCmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define STORAGE_NAMESPACE "ESP32_CONFIG"

#define START_CONFIG 0xDEADBEAF
#define END_CONFIG   0xBEAFDEAD

static const char* error_lvl_str[] =
  {
    [PRINT_DEBUG] = "DEBUG: ",
    [PRINT_INFO] = "INFO: ",
    [PRINT_WARNING] = "WARNING: ",
    [PRINT_ERROR] = "ERROR: " };

config_t config =
  {
    .start_config = START_CONFIG,
    .hw_ver = BOARD_VERSION,
    .sw_ver = SOFTWARE_VERSION,
    .dev_type = DEFAULT_DEV_TYPE,
    .end_config = END_CONFIG };

static xSemaphoreHandle mutexSemaphore;

uint8_t test_config_save( void );

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

int verify_config( config_t* conf )
{
  return 0;
}

static void configInitStruct( config_t* con )
{

}

void configInit( void )
{
  mutexSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive( mutexSemaphore );
}

int configSave( config_t* config )
{
  return 0;
}

int configRead( config_t* config )
{
  return 0;
}

void configRebootToBlt( void )
{
  esp_restart();
}
