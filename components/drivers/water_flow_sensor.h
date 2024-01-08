/**
 *******************************************************************************
 * @file    water_flow_sensor.h
 * @author  Dmytro Shevchenko
 * @brief   Water flow water sensor header file
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _WATER_FLOW_SENSOR_H_
#define _WATER_FLOW_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "error_code.h"

/* Public types --------------------------------------------------------------*/

typedef void ( *water_flow_sensor_alert_cb )( uint32_t value );

typedef struct
{
  const char* name;
  const char* unit;
  uint32_t counter;
  uint32_t value;
  water_flow_sensor_alert_cb alert_cb;
  uint32_t alert_value;
  uint8_t pin;
} water_flow_sensor_t;

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init input device.
 * @param   [in] dev - device pointer driver
 * @param   [in] name - sensor name
 * @param   [in] alert_cb - alert callback
 * @param   [in] pin - GPIO number
 */
void WaterFlowSensor_Init( water_flow_sensor_t* dev, const char* name, const char* unit, water_flow_sensor_alert_cb alert_cb, uint8_t pin );

/**
 * @brief   Water flow sensor reset value.
 * @param   [in] dev - device pointer driver
 */
error_code_t WaterFlowSensor_ResetValue( water_flow_sensor_t* dev );

/**
 * @brief   Water flow sensor set alert value.
 * @param   [in] dev - device pointer driver
 * @param   [in] alert_value - alert value
 * @return  error code
 */
error_code_t WaterFlowSensor_SetAlertValue( water_flow_sensor_t* dev, uint32_t alert_value );

/**
 * @brief   Water flow sensor set alert value.
 * @param   [in] dev - device pointer driver
 * @param   [out] buffer - buffer where write data
 * @param   [in] buffer_size - buffer size
 * @param   [in] is_add_comma - if true add comma on start of msg
 * 
 * @return  error code
 */
size_t WaterFlowSensor_GetStr( water_flow_sensor_t* dev, char* buffer, size_t buffer_size, bool is_add_comma );

#endif