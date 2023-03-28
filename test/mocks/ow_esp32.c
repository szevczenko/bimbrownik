/**
 *******************************************************************************
 * @file    ow.c
 * @author  Dmytro Shevchenko
 * @brief   One wire hal Layer
 *******************************************************************************
 */

#include "ow_esp32.h"

#include <string.h>

/* Private types -------------------------------------------------------------*/
typedef struct
{
} ow_ctx_t;

/* Private variables ---------------------------------------------------------*/
static ow_ctx_t ctx;

/* Private functions ---------------------------------------------------------*/
uint8_t OWUart_init( void* arg )
{
  return 1;
}

uint8_t OWUart_deinit( void* arg )
{
  return 1;
}

uint8_t OWUart_setBaudrate( uint32_t baud, void* arg )
{
  return 1;
}

uint8_t OWUart_transmitReceive( const uint8_t* tx, uint8_t* rx, size_t len, void* arg )
{
  return 1;
}
