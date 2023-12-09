/**
 *******************************************************************************
 * @file    dev_config.h
 * @author  Dmytro Shevchenko
 * @brief   Device configuration
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _DEV_CONFIG_H
#define _DEV_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/* Public functions ----------------------------------------------------------*/

/**
 * @brief   Init ota config.
 */
void DevConfig_Init( void );

/**
 * @brief   Get serial number.
 */
uint32_t DevConfig_GetSerialNumber( void );

#endif