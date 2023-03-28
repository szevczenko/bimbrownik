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
#define OW_UART_TXD      GPIO_NUM_27
#define OW_UART_RXD      GPIO_NUM_26
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

  uint8_t* tx;
  uint8_t* rx;
  size_t len;
  size_t rx_len;
  size_t tx_len;
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
  if ( ctx.dev->int_st.val & UART_INTR_RXFIFO_FULL )
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

  OW_ERROR_CHECK( uart_param_config( OW_UART_NUM, &uart_config ) );
  OW_ERROR_CHECK( uart_set_pin( OW_UART_NUM, OW_UART_TXD, OW_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE ) );
  uart_ll_ena_intr_mask( ctx.dev, UART_INTR_TX_DONE | UART_INTR_RXFIFO_FULL );
  int ret = esp_intr_alloc( uart_periph_signal[OW_UART_NUM].irq, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM, _uart_intr_handle, NULL, &ctx.handle_ow_uart );
  if ( ret != ESP_OK )
  {
    return 0;
  }
  ctx.rx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_REG( 0 ) :
                     ( ctx.dev == &UART1 ) ? UART_FIFO_REG( 1 ) :
                                             UART_FIFO_REG( 2 );
  ctx.tx_fifo_addr = ( ctx.dev == &UART0 ) ? UART_FIFO_AHB_REG( 0 ) :
                     ( ctx.dev == &UART1 ) ? UART_FIFO_AHB_REG( 1 ) :
                                             UART_FIFO_AHB_REG( 2 );
  ctx.dev->conf1.rxfifo_full_thrhd = 1;

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