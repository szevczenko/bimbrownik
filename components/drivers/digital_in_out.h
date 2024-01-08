/**
 *******************************************************************************
 * @file    digital_in_out.h
 * @author  Dmytro Shevchenko
 * @brief   Device input output which working with api
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _DIGITAL_IN_OUT_H_
#define _DIGITAL_IN_OUT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "error_code.h"

/* Public types --------------------------------------------------------------*/

typedef void ( *digital_in_interrupt_cb )( void );
typedef error_code_t ( *digital_set_value_cb )( bool value );

typedef struct
{
  const char* name;
  bool value;
  digital_set_value_cb set_value;
  uint8_t pin;
} digital_out_t;

typedef struct
{
  const char* name;
  bool value;
  digital_in_interrupt_cb interrupt;
  uint8_t pin;
} digital_in_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init input device.
 * @param   [in] dev - device pointer driver
 * @param   [in] name - output name
 * @param   [in] interrupt - interrupt callback
 * @param   [in] pin - GPIO number
 */
void DigitalIn_Init( digital_in_t* dev, const char* name, digital_in_interrupt_cb interrupt, uint8_t pin );

/**
 * @brief   Digital out set value.
 * @param   [in] dev - device pointer driver
 */
error_code_t DigitalIn_ReadValue( digital_in_t* dev );

/**
 * @brief   Init output device.
 * @param   [in] dev - device pointer driver
 * @param   [in] name - output name
 * @param   [in] set_value_callback - set callback value
 * @param   [in] pin - GPIO number
 */
void DigitalOut_Init( digital_out_t* dev, const char* name, digital_set_value_cb set_value_callback, uint8_t pin );

/**
 * @brief   Digital out set value.
 * @param   [in] dev - device pointer driver
 * @param   [in] value - value
 */
error_code_t DigitalOut_SetValue( digital_out_t* dev, bool value );

#endif