/**
 *******************************************************************************
 * @file    analog_in.h
 * @author  Dmytro Shevchenko
 * @brief   Analog input sensor
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _ANALOG_IN_H_
#define _ANALOG_IN_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "error_code.h"

/* Public types --------------------------------------------------------------*/

typedef error_code_t ( *analog_init_cb )( void );
typedef error_code_t ( *analog_read_value_cb )( uint32_t* value );
typedef error_code_t ( *analog_deinit_cb )( void );

typedef struct
{
  const char* name;
  const char* unit;
  uint32_t value;
  analog_init_cb init;
  analog_read_value_cb read_value;
  analog_deinit_cb deinit;
} analog_in_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init input device.
 * @param   [in] dev - device pointer driver
 * @param   [in] name - output name
 * @param   [in] read_value_callback - read callback value
 */
void AnalogIn_Init( analog_in_t* dev, const char* name, const char* unit, analog_init_cb init,
                    analog_read_value_cb read_value, analog_deinit_cb deinit );

/**
 * @brief   Digital out set value.
 * @param   [in] dev - device pointer driver
 */
error_code_t AnalogIn_ReadValue( analog_in_t* dev );

#endif