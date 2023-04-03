/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"
#include "lvgl_spi_conf.h"
#include "lvgl_tft/disp_spi.h"
#include "lvgl_touch/tp_spi.h"
#include "ui.h"

/*********************
 *      DEFINES
 *********************/
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task( void* arg );
static void guiTask( void* pvParameter );

/**********************
 *   APPLICATION MAIN
 **********************/
void screenInit( void )
{
  /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
  xTaskCreatePinnedToCore( guiTask, "gui", 4096 * 2, NULL, 0, NULL, 1 );
}

static SemaphoreHandle_t xGuiSemaphore;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t* buf1[DISP_BUF_SIZE];
static lv_color_t* buf2 = NULL;
static lv_disp_drv_t disp_drv;

static void guiTask( void* pvParameter )
{
  (void) pvParameter;
  xGuiSemaphore = xSemaphoreCreateMutex();

  lv_init();
  lvgl_driver_init();
  lv_disp_draw_buf_init( &disp_buf, buf1, buf2, DISP_BUF_SIZE );

  lv_disp_drv_init( &disp_drv );
  disp_drv.flush_cb = disp_driver_flush;
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 480;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register( &disp_drv );

  /* Register an input device when enabled on the menuconfig */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
  lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.read_cb = touch_driver_read;
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  lv_indev_drv_register( &indev_drv );
#endif

  /* Create and start a periodic timer interrupt to call lv_tick_inc */
  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &lv_tick_task,
    .name = "periodic_gui" };
  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK( esp_timer_create( &periodic_timer_args, &periodic_timer ) );
  ESP_ERROR_CHECK( esp_timer_start_periodic( periodic_timer, LV_TICK_PERIOD_MS * 1000 ) );

  /* Create the demo application */
  ui_init();

  while ( 1 )
  {
    /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
    vTaskDelay( pdMS_TO_TICKS( 10 ) );

    /* Try to take the semaphore, call lvgl related function on success */
    if ( pdTRUE == xSemaphoreTake( xGuiSemaphore, portMAX_DELAY ) )
    {
      lv_task_handler();
      xSemaphoreGive( xGuiSemaphore );
    }
  }
}

static void lv_tick_task( void* arg )
{
  (void) arg;

  lv_tick_inc( LV_TICK_PERIOD_MS );
}