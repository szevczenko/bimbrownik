#include <stdbool.h>
#include <stdio.h>

#include "app_events.h"
#include "app_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network_manager.h"
#include "screen.h"
#include "wifidrv.h"
#include "temperature.h"

#include "ow_esp32.h"
#include "ow/ow.h"
#include "ow/devices/ow_device_ds18x20.h"

void app_main( void )
{
  // printf( "Hello World\n\r" );
  configInit();
  wifiDrvInit( WIFI_TYPE_DEVICE );
  NetworkManagerInit();
  AppManagerInit();
  TemperatureInit();
  // screenInit();

  // ow_init( &ow, &ow_ll_drv_esp32, NULL ); /* Initialize 1-Wire library and set user argument to NULL */
  // uint8_t tx[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  // uint8_t rx[8];
  // // OWUart_transmitReceive(tx, rx, 8, NULL);

  // /* Get onewire devices connected on 1-wire port */
  // do
  // {
  //   if ( ow_search_devices( &ow, rom_ids, OW_ARRAYSIZE( rom_ids ), &rom_found ) == owOK )
  //   {
  //     printf( "Devices scanned, found %d devices!\r\n", (int) rom_found );
  //   }
  //   else
  //   {
  //     printf( "Device scan error\r\n" );
  //   }
  //   if ( rom_found == 0 )
  //   {
  //     vTaskDelay( 1000 );
  //   }
  // } while ( rom_found == 0 );

  // if ( rom_found )
  // {
  //   /* Infinite loop */
  //   while ( 1 )
  //   {
  //     printf( "Start temperature conversion\r\n" );
  //     ow_ds18x20_start( &ow, NULL ); /* Start conversion on all devices, use protected API */
  //     vTaskDelay( 1000 ); /* Release thread for 1 second */

  //     /* Read temperature on all devices */
  //     avg_temp = 0;
  //     avg_temp_count = 0;
  //     for ( size_t i = 0; i < rom_found; i++ )
  //     {
  //       if ( ow_ds18x20_is_b( &ow, &rom_ids[i] ) )
  //       {
  //         float temp;
  //         uint8_t resolution = ow_ds18x20_get_resolution( &ow, &rom_ids[i] );
  //         if ( ow_ds18x20_read( &ow, &rom_ids[i], &temp ) )
  //         {
  //           printf( "Sensor %02u temperature is %d.%d degrees (%u bits resolution)\r\n",
  //                   (unsigned) i, (int) temp, (int) ( ( temp * 1000.0f ) - ( ( (int) temp ) * 1000 ) ), (unsigned) resolution );

  //           avg_temp += temp;
  //           avg_temp_count++;
  //         }
  //         else
  //         {
  //           printf( "Could not read temperature on sensor %u\r\n", (unsigned) i );
  //         }
  //       }
  //     }
  //     if ( avg_temp_count > 0 )
  //     {
  //       avg_temp = avg_temp / avg_temp_count;
  //     }
  //     printf( "Average temperature: %d.%d degrees\r\n", (int) avg_temp, (int) ( ( avg_temp * 100.0f ) - ( (int) avg_temp ) * 100 ) );
  //   }
  // }
}
