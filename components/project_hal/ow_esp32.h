/**
 *******************************************************************************
 * @file    ow.h
 * @author  Dmytro Shevchenko
 * @brief   One wire hal Layer
 *******************************************************************************
 */

/* Define to prevent recursive inclusion ------------------------------------*/

#ifndef _OW_H
#define _OW_H

#include <stddef.h>
#include <stdint.h>

/* Public functions ----------------------------------------------------------*/
uint8_t OWUart_init( void* arg );
uint8_t OWUart_deinit( void* arg );
uint8_t OWUart_setBaudrate( uint32_t baud, void* arg );
uint8_t OWUart_transmitReceive( const uint8_t* tx, uint8_t* rx, size_t len, void* arg );

#endif