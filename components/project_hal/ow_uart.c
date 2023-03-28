/**
 *******************************************************************************
 * @file    ow.c
 * @author  Dmytro Shevchenko
 * @brief   One wire hal Layer
 *******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "ow_uart.h"

#include <soc/uart_struct.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/semphr.h"
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
    return OW_ERR_FAIL;         \
  }

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY( byte )   \
  ( byte & 0x80 ? '1' : '0' ),   \
    ( byte & 0x40 ? '1' : '0' ), \
    ( byte & 0x20 ? '1' : '0' ), \
    ( byte & 0x10 ? '1' : '0' ), \
    ( byte & 0x08 ? '1' : '0' ), \
    ( byte & 0x04 ? '1' : '0' ), \
    ( byte & 0x02 ? '1' : '0' ), \
    ( byte & 0x01 ? '1' : '0' )

#define OW_9600_BAUDRATE   9600
#define OW_115200_BAUDRATE 115200
#define OW_SIGNAL_0        0x00    // 0x00 --default
#define OW_SIGNAL_1        0xff
#define OW_SIGNAL_READ     0xff
#define ONEWIRE_RESET      0xF0

#define OW_UART_CONFIG( baudrate )         \
  {                                        \
    .baud_rate = baudrate,                 \
    .data_bits = UART_DATA_8_BITS,         \
    .parity = UART_PARITY_DISABLE,         \
    .stop_bits = UART_STOP_BITS_1,         \
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
    .source_clk = UART_SCLK_APB,           \
  }

#define WAIT_UNTIL_DONE( what_we_wait ) \
  uint16_t _d = 0xffff;                 \
  while ( !what_we_wait && _d-- )       \
    asm( "nop" );

/* Private types -------------------------------------------------------------*/
typedef struct
{
  uart_dev_t* dev;
  uint32_t rx_fifo_addr;
  uint32_t tx_fifo_addr;
  uint8_t rx;
  uint32_t last_baud_rate;
  uart_isr_handle_t* handle_ow_uart;
  bool tx_done;
  bool rx_done;
} ow_ctx_t;

/* Private variables ---------------------------------------------------------*/
static ow_ctx_t ctx;

/* Private functions ---------------------------------------------------------*/
static void IRAM_ATTR _uart_intr_handle( void* arg )
{
  uint16_t _len = uart_ll_get_rxfifo_len( ctx.dev );
  if ( ctx.dev->int_st.val & UART_INTR_TX_DONE )
  {
    ctx.tx_done = true;
  }
  if ( ctx.dev->int_st.val & UART_INTR_RXFIFO_FULL )
  {
    while ( _len )
    {
      ctx.rx = READ_PERI_REG( ctx.rx_fifo_addr );
      _len -= 1;
    }
    ctx.rx_done = true;
  }
  uart_clear_intr_status( OW_UART_NUM, UART_LL_INTR_MASK );
}

/* Public functions ----------------------------------------------------------*/
ow_err_t OW_Init( void )
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
  // uart_driver_install(OW_UART_NUM, 256, 0, 0, NULL, 0);
  OW_ERROR_CHECK( uart_param_config( OW_UART_NUM, &uart_config ) );
  OW_ERROR_CHECK( uart_set_pin( OW_UART_NUM, OW_UART_TXD, OW_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE ) );
  uart_ll_ena_intr_mask( ctx.dev, UART_INTR_TX_DONE | UART_INTR_RXFIFO_FULL );
  // OW_ERROR_CHECK( uart_isr_register( OW_UART_NUM, _uart_intr_handle, NULL,
  //                                    0,
  //                                    ctx.handle_ow_uart ) );
  int ret = esp_intr_alloc( uart_periph_signal[OW_UART_NUM].irq, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM, _uart_intr_handle, NULL, NULL );
  if ( ret != ESP_OK )
  {
    return OW_ERR_FAIL;
  }
  ctx.rx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_REG( 0 ) :
                     ( ctx.dev == &UART1 ) ? UART_FIFO_REG( 1 ) :
                                             UART_FIFO_REG( 2 );
  ctx.tx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_AHB_REG( 0 ) :
                     ( ctx.dev == &UART1 ) ? UART_FIFO_AHB_REG( 1 ) :
                                             UART_FIFO_AHB_REG( 2 );
  ctx.dev->conf1.rxfifo_full_thrhd = 1;
  return OW_ERR_OK;
}

static ow_err_t _ow_uart_write_byte( uint32_t baudrate, uint8_t data )
{
  if ( ctx.last_baud_rate != baudrate )
  {
    WAIT_UNTIL_DONE( ctx.tx_done );
    OW_ERROR_CHECK( uart_set_baudrate( OW_UART_NUM, baudrate ) );
    ctx.last_baud_rate = baudrate;
  }
  ctx.rx = 0x00;
  ctx.tx_done = false;
  ctx.rx_done = false;
  WRITE_PERI_REG( ctx.tx_fifo_addr, data );
  return OW_ERR_OK;
}

static uint32_t _ow_uart_read( void )
{
  WAIT_UNTIL_DONE( ctx.rx_done )
  return ctx.rx;
}

uint16_t ow_uart_reset( void )
{
  if ( _ow_uart_write_byte( OW_9600_BAUDRATE, ONEWIRE_RESET ) != OW_ERR_OK )
  {
    return 0;
  }
  return ( _ow_uart_read() != ONEWIRE_RESET ) ? 0x01 : 0x00;
}

void ow_uart_send_signal( uint16_t data )
{
  uint32_t _duration_as_uart_data = ( data ) ? OW_SIGNAL_1 : OW_SIGNAL_0;
  _ow_uart_write_byte( OW_115200_BAUDRATE, _duration_as_uart_data );
}

uint16_t ow_uart_read_signal( void )
{
  if ( _ow_uart_write_byte( OW_9600_BAUDRATE, OW_SIGNAL_READ ) != OW_ERR_OK )
  {
    return 0;
  }
  return ( _ow_uart_read() == OW_SIGNAL_READ ) ? 0x01 : 0x00;
}
