/*
pcf8574 lib 0x01

copyright (c) Davide Gironi, 2012

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/

#ifndef PCF8574_H_
#define PCF8574_H_

#include <stdint.h>

#define PCF8574_ADDRBASE ( 0x40 )    //device base address

#define PCF8574_MAXDEVICES 8    //max devices, depends on address (3 bit)
#define PCF8574_MAXPINS    8    //max pin per device

//pin status
extern uint8_t pcf8574_pinstatus[PCF8574_MAXDEVICES];

//functions
void pcf8574_init();
extern uint8_t pcf8574_getoutput( uint8_t deviceid );
extern uint8_t pcf8574_getoutputpin( uint8_t deviceid, uint8_t pin );
extern uint8_t pcf8574_setoutput( uint8_t deviceid, uint8_t data );
extern uint8_t pcf8574_setoutputpins( uint8_t deviceid, uint8_t pinstart, uint8_t pinlength, uint8_t data );
extern uint8_t pcf8574_setoutputpin( uint8_t deviceid, uint8_t pin, uint8_t data );
extern uint8_t pcf8574_setoutputpinhigh( uint8_t deviceid, uint8_t pin );
extern uint8_t pcf8574_setoutputpinlow( uint8_t deviceid, uint8_t pin );
extern int pcf8574_getinput( uint8_t deviceid );
extern uint8_t pcf8574_getinputpin( uint8_t deviceid, uint8_t pin );
#endif
