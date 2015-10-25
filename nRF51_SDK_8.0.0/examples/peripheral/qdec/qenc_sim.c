/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "nrf_gpio.h"
#include "app_gpiote.h"
#include "nrf_error.h"
#include "nrf_qdec.h"
#include "qenc_sim.h"


/**
 * @enum nrf_qdec_task_t
 * @brief enumeration of QDEC tasks.
 * coding is [A|B] i.e. B is LSB
 */

#define _static

typedef enum {
  state_0 = 0,      /**< AB=00 */
  state_1 = 1,      /**< AB=01 */
  state_2 = 2,      /**< AB=10 */
  state_3 = 3       /**< AB=11 */
} qenc_state_t;

const qenc_state_t next_pos[4] = { state_1, state_3, state_0, state_2 }; // change of state to get phase increment
const qenc_state_t next_neg[4] = { state_2, state_0, state_3, state_1 }; // change of state to get phase decrement
const qenc_state_t next_dbl[4] = { state_3, state_2, state_1, state_0 }; // change of state to get double transition

_static volatile qenc_state_t qenc_state = state_0;
_static volatile int32_t qenc_count = 0;
_static volatile uint32_t qenc_dbl_count = 0;
_static volatile bool  qenc_enable_flag = false;

static void gpiote_event_handler(void)
{
    if ((qenc_count > 0) && qenc_enable_flag)
    {
      qenc_count--;
      qenc_state = next_pos[qenc_state];

    }
    else if ((qenc_count < 0 ) && qenc_enable_flag )
    {
      qenc_count++;
      qenc_state = next_neg[qenc_state];
    }
    else if ((qenc_dbl_count > 0 ) && qenc_enable_flag)
    {
      qenc_dbl_count--;
      qenc_state = next_dbl[qenc_state];
    }
    else
    {
      qenc_enable_flag = false;
    }

    nrf_gpio_pin_write(QENC_CONFIG_PIO_B, qenc_state & 0x01);
    nrf_gpio_pin_write(QENC_CONFIG_PIO_A, (qenc_state & 0x02) >> 1 );
}

static void qenc_init_gpio(void)
{
    nrf_gpio_cfg_input(QENC_CONFIG_PIO_LED, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(QENC_CONFIG_PIO_A);
    nrf_gpio_cfg_output(QENC_CONFIG_PIO_B);
}

static void qenc_init_gpiote(uint8_t channel, nrf_qdec_ledpol_t led_pol)
{
    // change state on inactive edge of led pulse
    if (led_pol == NRF_QDEC_LEPOL_ACTIVE_HIGH)
    {
      (void)app_gpiote_input_event_handler_register(channel,
                                                    QENC_CONFIG_PIO_LED,
                                                    GPIOTE_CONFIG_POLARITY_LoToHi,
                                                    gpiote_event_handler);
    }
    else
    {
      (void)app_gpiote_input_event_handler_register(channel,
                                                    QENC_CONFIG_PIO_LED,
                                                    GPIOTE_CONFIG_POLARITY_HiToLo,
                                                    gpiote_event_handler);
    }
    (void)app_gpiote_enable_interrupts();

}

void qenc_init(uint8_t channel, nrf_qdec_ledpol_t led_pol)
{
    qenc_init_gpio();
    qenc_init_gpiote(channel, led_pol);
}    

// this function is used mainly in tests
void qenc_pulse_dblpulse_count_set(int32_t pulse_count, uint32_t dble_pulse_count)
{
    qenc_count = pulse_count;
    qenc_dbl_count = dble_pulse_count;
    qenc_enable_flag = true;
}

// this function is used mainly in a example
void qenc_pulse_count_set(int32_t pulse_count)
{
    qenc_pulse_dblpulse_count_set(pulse_count, 0);
}

