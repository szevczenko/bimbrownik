/*
pcf8574 lib 0x01

copyright (c) Davide Gironi, 2012

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/

#include "pcf8574.h"

#include "app_config.h"
/*
 * initialize
 */
void pcf8574_init()
{
}

/*
 * get output status
 */
uint8_t pcf8574_getoutput( uint8_t deviceid )
{
  uint8_t data = -1;
  return data;
}

/*
 * get output pin status
 */
uint8_t pcf8574_getoutputpin( uint8_t deviceid, uint8_t pin )
{
  uint8_t data = -1;
  return data;
}

/*
 * set output pins
 */
uint8_t pcf8574_setoutput( uint8_t deviceid, uint8_t data )
{
  return -1;
}

/*
 * set output pins, replace actual status of a device from pinstart for pinlength with data
 */
uint8_t pcf8574_setoutputpins( uint8_t deviceid, uint8_t pinstart, uint8_t pinlength, uint8_t data )
{
  return -1;
}

/*
 * set output pin
 */
uint8_t pcf8574_setoutputpin( uint8_t deviceid, uint8_t pin, uint8_t data )
{
  return -1;
}

/*
 * set output pin high
 */
uint8_t pcf8574_setoutputpinhigh( uint8_t deviceid, uint8_t pin )
{
  return pcf8574_setoutputpin( deviceid, pin, 1 );
}

/*
 * set output pin low
 */
uint8_t pcf8574_setoutputpinlow( uint8_t deviceid, uint8_t pin )
{
  return pcf8574_setoutputpin( deviceid, pin, 0 );
}

/*
 * get input data
 */
int pcf8574_getinput( uint8_t deviceid )
{
  int data = 0;
  return data;
}

/*
 * get input pin (up or low)
 */
uint8_t pcf8574_getinputpin( uint8_t deviceid, uint8_t pin )
{
  uint8_t data = -1;
  return data;
}
