/**
 *******************************************************************************
 * @file    ow.c
 * @author  Dmytro Shevchenko
 * @brief   One wire hal Layer
 *******************************************************************************
 */

#include "ow_esp32.h"

#include <soc/uart_struct.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "hal/uart_hal.h"
#include "hal/uart_ll.h"

/* Private macros ------------------------------------------------------------*/
#define MODULE_NAME "[OWHal] "
#define DEBUG_LVL   PRINT_INFO

#if CONFIG_DEBUG_OW
#define LOG( _lvl, ... ) \
  debug_printf( DEBUG_LVL, _lvl, MODULE_NAME __VA_ARGS__ )
#else
#define LOG( PRINT_INFO, ... )
#endif

#define OW_ERROR_CHECK( _func ) \
  if ( ( _func ) != ESP_OK )    \
  {                             \
    return 0;                   \
  }

#define OW_UART_CONFIG( baudrate )         \
  {                                        \
    .baud_rate = baudrate,                 \
    .data_bits = UART_DATA_8_BITS,         \
    .parity = UART_PARITY_DISABLE,         \
    .stop_bits = UART_STOP_BITS_1,         \
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
    .source_clk = UART_SCLK_APB,           \
  }

#define OW_UART_NUM      UART_NUM_1
#define OW_UART_TXD      GPIO_NUM_37
#define OW_UART_RXD      GPIO_NUM_18
#define OW_9600_BAUDRATE 9600
#define OW_UNUSED( x )   ( (void) ( x ) ) /*!< Unused variable macro */

/* Private types -------------------------------------------------------------*/
typedef struct
{
  uart_dev_t* dev;
  uint32_t rx_fifo_addr;
  uint32_t tx_fifo_addr;
  uint32_t last_baud_rate;
  intr_handle_t handle_ow_uart;
  int tx_pin;
  int rx_pin;

  uint8_t* tx;
  uint8_t* rx;
  size_t len;
  size_t rx_len;
  size_t tx_len;
  bool tx_done;
  bool rx_done;
} ow_ctx_t;

/* Private variables ---------------------------------------------------------*/
static ow_ctx_t ctx = {
  .rx_pin = OW_UART_RXD,
  .tx_pin = OW_UART_TXD };

/* Private functions ---------------------------------------------------------*/
static void IRAM_ATTR _uart_intr_handle( void* arg )
{
  uint16_t _len = uart_ll_get_rxfifo_len( ctx.dev );
  uint32_t uart_intr_status = uart_ll_get_intsts_mask( ctx.dev );
  if ( uart_intr_status & UART_INTR_TX_DONE )
  {
    if ( ctx.tx_len == ctx.len )
    {
      ctx.tx_done = true;
    }
    else
    {
      WRITE_PERI_REG( ctx.tx_fifo_addr, ctx.tx[ctx.tx_len] );
      ctx.tx_len++;
    }
    uart_clear_intr_status( OW_UART_NUM, UART_INTR_TX_DONE );
  }
  if ( uart_intr_status & UART_INTR_RXFIFO_FULL )
  {
    while ( _len && ( ctx.rx_done == false ) && ctx.rx_len < ctx.len )
    {
      ctx.rx[ctx.rx_len] = READ_PERI_REG( ctx.rx_fifo_addr );
      ctx.rx_len++;
      _len -= 1;
    }
    if ( ctx.rx_len == ctx.len )
    {
      ctx.rx_done = true;
    }
    uart_clear_intr_status( OW_UART_NUM, UART_INTR_RXFIFO_FULL );
  }
}

/* Public functions ---------------------------------------------------------*/
void OWUart_SetPin( int rx, int tx )
{
  ctx.tx_pin = tx;
  ctx.rx_pin = rx;
}

uint8_t OWUart_init( void* arg )
{
  ctx.dev = UART_LL_GET_HW( OW_UART_NUM );
  ctx.last_baud_rate = OW_9600_BAUDRATE;
  ctx.rx = 0x00;
  ctx.handle_ow_uart = NULL;
  ctx.tx_done = true;
  ctx.rx_done = true;
  assert( ctx.tx_done );
  assert( ctx.rx_done );
  uart_config_t uart_config = OW_UART_CONFIG( ctx.last_baud_rate );

  ///////////////////////
  //zero-initialize the config structure.
  gpio_config_t io_conf = {};
  //disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  //set as output mode
  io_conf.mode = GPIO_MODE_INPUT;
  //bit mask of the pins that you want to set,e.g.GPIO18/19
  io_conf.pin_bit_mask = ( /*( 1ULL << 37 ) |*/ ( 1ULL << 36 ) | ( 1ULL << 35 ) | ( 1ULL << 34 ) | ( 1ULL << 33 ) );
  //disable pull-down mode
  io_conf.pull_down_en = 0;
  //disable pull-up mode
  io_conf.pull_up_en = 0;
  //configure GPIO with the given settings
  gpio_config( &io_conf );
  gpio_set_level( 36, 0 );
  gpio_set_level( 35, 0 );
  gpio_set_level( 34, 0 );
  gpio_set_level( 33, 0 );
  ///////////////////////

  OW_ERROR_CHECK( uart_param_config( OW_UART_NUM, &uart_config ) );
  OW_ERROR_CHECK( uart_set_pin( OW_UART_NUM, OW_UART_TXD, OW_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE ) );

  uart_ll_ena_intr_mask( ctx.dev, UART_INTR_TX_DONE | UART_INTR_RXFIFO_FULL );
  int ret = esp_intr_alloc( uart_periph_signal[OW_UART_NUM].irq, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM, _uart_intr_handle, NULL, &ctx.handle_ow_uart );
  if ( ret != ESP_OK )
  {
    return 0;
  }
  ctx.rx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_AHB_REG( 0 ) : UART_FIFO_AHB_REG( 1 );
  ctx.tx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_AHB_REG( 0 ) : UART_FIFO_AHB_REG( 1 );
  uart_ll_set_rxfifo_full_thr( ctx.dev, 1 );
  OW_UNUSED( arg );

  return 1;
}

uint8_t OWUart_deinit( void* arg )
{
  esp_intr_free( ctx.handle_ow_uart );
  uart_disable_rx_intr( OW_UART_NUM );
  uart_disable_tx_intr( OW_UART_NUM );
  OW_UNUSED( arg );
  return 1;
}

uint8_t OWUart_setBaudrate( uint32_t baud, void* arg )
{
  uart_set_baudrate( OW_UART_NUM, baud );
  OW_UNUSED( arg );

  return 1;
}

uint8_t OWUart_transmitReceive( const uint8_t* tx, uint8_t* rx, size_t len, void* arg )
{
  ctx.tx_done = false;
  ctx.rx_done = false;
  ctx.len = len;
  ctx.rx = rx;
  ctx.tx = (uint8_t*) tx;
  ctx.rx_len = 0;
  ctx.tx_len = 0;

  WRITE_PERI_REG( ctx.tx_fifo_addr, tx[ctx.tx_len] );
  ctx.tx_len++;

  uint32_t wait = 0xfffff;
  while ( ( !ctx.rx_done ) && ( wait > 0 ) )
  {
    wait--;
  }

  OW_UNUSED( arg );
  return 1;
}

#if 0
#include "ow.h"

void OWUart_Test( void )
{
  const ow_ll_drv_t ow_ll_drv_esp32 = {
    .init = OWUart_init,
    .deinit = OWUart_deinit,
    .set_baudrate = OWUart_setBaudrate,
    .tx_rx = OWUart_transmitReceive,
  };
  ow_t ow;
  ow_rom_t rom_ids[10];
  size_t rom_found;
  float avg_temp;
  size_t avg_temp_count;

  ow_init( &ow, &ow_ll_drv_esp32, NULL ); /* Initialize 1-Wire library and set user argument to NULL */

  /* Get onewire devices connected on 1-wire port */
  do
  {
    if ( ow_search_devices( &ow, rom_ids, OW_ARRAYSIZE( rom_ids ), &rom_found ) == owOK )
    {
      printf( "Devices scanned, found %d devices!\r\n", (int) rom_found );
    }
    else
    {
      printf( "Device scan error\r\n" );
    }
    if ( rom_found == 0 )
    {
      vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
  } while ( rom_found == 0 );

  if ( rom_found )
  {
    /* Infinite loop */
    while ( 1 )
    {
      printf( "Start temperature conversion\r\n" );
      ow_ds18x20_start( &ow, NULL ); /* Start conversion on all devices, use protected API */
      vTaskDelay( 1000 / portTICK_PERIOD_MS ); /* Release thread for 1 second */

      /* Read temperature on all devices */
      avg_temp = 0;
      avg_temp_count = 0;
      for ( size_t i = 0; i < rom_found; i++ )
      {
        if ( ow_ds18x20_is_b( &ow, &rom_ids[i] ) )
        {
          float temp;
          uint8_t resolution = ow_ds18x20_get_resolution( &ow, &rom_ids[i] );
          if ( ow_ds18x20_read( &ow, &rom_ids[i], &temp ) )
          {
            printf( "Sensor %02u temperature is %d.%d degrees (%u bits resolution)\r\n",
                    (unsigned) i, (int) temp, (int) ( ( temp * 1000.0f ) - ( ( (int) temp ) * 1000 ) ), (unsigned) resolution );

            avg_temp += temp;
            avg_temp_count++;
          }
          else
          {
            printf( "Could not read temperature on sensor %u\r\n", (unsigned) i );
          }
        }
      }
      if ( avg_temp_count > 0 )
      {
        avg_temp = avg_temp / avg_temp_count;
      }
      printf( "Average temperature: %d.%d degrees\r\n", (int) avg_temp, (int) ( ( avg_temp * 100.0f ) - ( (int) avg_temp ) * 100 ) );
    }
  }
}
#endif
