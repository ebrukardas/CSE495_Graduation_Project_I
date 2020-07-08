/*! ---------------------------------------------------------------------------
*  @file    main.c
*  @brief   Two Way Time Of Arrival(Flight) - Anchor
*
*           Bu dosya TOF yapilacak projenin anchor(sabit istasyon) kismidir. 
*           Poll mesaji beklenir, geldiginde RX time stamp'i alinir ve 
*           TX time stamp'i kaydedilerek cevap(anchonse) mesaji yayinlanir.
*
*/

// ====================== KUTUPHANELER ======================

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "bsp.h"
#include "boards.h"
#include "nordic_common.h"
#include "nrf_drv_clock.h"
#include "nrf_drv_spi.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf.h"
#include "app_error.h"
#include <string.h>

#include "port_platform.h"
#include "deca_types.h"
#include "deca_param_types.h"
#include "deca_regs.h"
#include "deca_device_api.h"

// ====================== TANIMLAMALAR ======================


// dw1000 için kullanilan yapilandirma degiskenleri
static dwt_config_t config = {
  5,                /* Channel number. */
  DWT_PRF_64M,      /* Pulse repetition frequency. */
  DWT_PLEN_128,     /* Preamble length. Used in TX only. */
  DWT_PAC8,         /* Preamble acquisition chunk size. Used in RX only. */
  10,               /* TX preamble code. Used in TX only. */
  10,               /* RX preamble code. Used in RX only. */
  0,                /* 0 to use standard SFD, 1 to use non-standard SFD. */
  DWT_BR_6M8,       /* Data rate. */
  DWT_PHRMODE_STD,  /* PHY header mode. */
  (129 + 8 - 8)     /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
};



#define TASK_DELAY        10
#define TIMER_PERIOD      2000

#ifdef USE_FREERTOS
  TaskHandle_t  ss_anchor_task_handle;
  extern void ss_anchor_task_function (void * pvParameter);
  TaskHandle_t  led_toggle_task_handle;
  TimerHandle_t led_toggle_timer_handle;
#else
  extern int ss_anch_run(void);
#endif    // USE_FREERTOS


// ====================== UYGULAMA ======================


#ifdef USE_FREERTOS

  /**@brief led icin task
  *
  * @param[in] pvParameter
  */

  static void led_toggle_task_function (void * pvParameter)
  {
    UNUSED_PARAMETER(pvParameter);
    while (true)
    {
      LEDS_INVERT(BSP_LED_0_MASK);
      vTaskDelay(TASK_DELAY);
    }
  }

  /**@brief FreeRTOS timer'i doldugunda cagirilacak
  *
  * @param[in] pvParameter
  */
  static void led_toggle_timer_callback (void * pvParameter)
  {
    UNUSED_PARAMETER(pvParameter);
    LEDS_INVERT(BSP_LED_1_MASK);
}

#endif  // USE_FREERTOS

int main(void)
{
	// led ayarlamalari
  LEDS_CONFIGURE(BSP_LED_0_MASK | BSP_LED_1_MASK);
  LEDS_ON(BSP_LED_0_MASK | BSP_LED_1_MASK);

  #ifdef USE_FREERTOS
    UNUSED_VARIABLE(xTaskCreate(led_toggle_task_function, "LED0", configMINIMAL_STACK_SIZE + 200, NULL, 2, &led_toggle_task_handle));

    led_toggle_timer_handle = xTimerCreate( "LED1", TIMER_PERIOD, pdTRUE, NULL, led_toggle_timer_callback);
    UNUSED_VARIABLE(xTimerStart(led_toggle_timer_handle, 0));

    UNUSED_VARIABLE(xTaskCreate(ss_anchor_task_function, "SSTWR_anch", configMINIMAL_STACK_SIZE + 200, NULL, 2, &ss_anchor_task_handle)); 
  #endif  // USE_FREERTOS

	// dw1000 init
	
  nrf_gpio_cfg_input(DW1000_IRQ, NRF_GPIO_PIN_NOPULL); 		//irq
  reset_DW1000(); // Reset DW1000
  port_set_dw1000_slowrate(); // 2MHz 

  if (dwt_initialise(DWT_LOADUCODE) == DWT_ERROR)
  {
    while (1)
    {};
  }

  port_set_dw1000_fastrate();    // 8MHz 

	// dw1000 conf
  dwt_configure(&config);

  // TX ve RX için default anten gecikmesi
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);

  dwt_setrxtimeout(0);

  #ifdef USE_FREERTOS
    vTaskStartScheduler();	

    while(1)
    {}
  #else
    while (1)
    {
      ss_anch_run();
    }
    #endif  // USE_FREERTOS
}