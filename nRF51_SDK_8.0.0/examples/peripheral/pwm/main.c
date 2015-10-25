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

/** @file
 * @defgroup pwm_example_main main.c
 * @{
 * @ingroup pwm_example
 * 
 * @brief  PWM Example Application main file.
 *
 * This file contains the source code for a sample application using PWM.
 *
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "app_timer.h"
#include "app_gpiote.h"
#include "nrf_gpiote.h"
#include "nrf_gpio.h"
#include "bsp.h"

#define APP_TIMER_PRESCALER      0                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS     (1 + BSP_APP_TIMERS_NUMBER) /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE  2                           /**< Size of timer operation queues. */

#define BUTTON_PREV_ID           0
#define BUTTON_NEXT_ID           1

#ifdef BSP_LED_0
    #define PWM_OUTPUT_PIN_NUMBER (BSP_LED_0)               /**< Pin number for PWM output. */
#endif
#ifndef PWM_OUTPUT_PIN_NUMBER
    #error "Please define PWM_OUTPUT_PIN_NUMBER"
#endif

#define MAX_SAMPLE_LEVELS        (256)               /**< Maximum number of sample levels. */
#define INCREMENT_VALUE          (16)                /**< Value by which the pulse_width is incremented/decremented for each event. */
#define TIMER_PRESCALERS         (6U)                /**< Prescaler setting for timer. */

int duty_cycle =                 1;                  /**< Fill of pwm signal (in range <1,MAX_SAMPLE_LEVELS-1>). */

/**@brief Function for handling bsp events.
 */
void bsp_evt_handler(bsp_event_t evt)
{
    switch (evt)
    {
        case BSP_EVENT_KEY_0:
            duty_cycle -= INCREMENT_VALUE;

            if ( duty_cycle <= 0 )
                duty_cycle = 1;
            break;

        case BSP_EVENT_KEY_1:
            duty_cycle += INCREMENT_VALUE;

            if ( duty_cycle >= MAX_SAMPLE_LEVELS )
                duty_cycle = MAX_SAMPLE_LEVELS - 1;
            break;

        default:
            break; // no implementation needed
    }
}


/** @brief Function for handling timer 2 peripheral interrupts.
 */
void TIMER2_IRQHandler(void)
{
    static bool cc0_turn = false; /**< Keeps track of which CC register to be used. */

    if ((NRF_TIMER2->EVENTS_COMPARE[1] != 0) &&
        ((NRF_TIMER2->INTENSET & TIMER_INTENSET_COMPARE1_Msk) != 0))
    {
        // Sets the next CC1 value
        NRF_TIMER2->EVENTS_COMPARE[1] = 0;
        NRF_TIMER2->CC[1]             = (NRF_TIMER2->CC[1] + MAX_SAMPLE_LEVELS);

        // Every other interrupt CC0 and CC2 will be set to their next values.
        if (cc0_turn)
        {
            NRF_TIMER2->CC[0] = NRF_TIMER2->CC[1] + duty_cycle;
        }
        else
        {
            NRF_TIMER2->CC[2] = NRF_TIMER2->CC[1] + duty_cycle;
        }
        // Next turn the other CC will get its value.
        cc0_turn = !cc0_turn;
    }
}


/** @brief Function for initializing the Timer 2 peripheral.
 */
static void timer2_init(void)
{
    // Start 16 MHz crystal oscillator .
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1;

    // Wait for the external oscillator to start up.
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // Do nothing.
    }

    NRF_TIMER2->MODE      = TIMER_MODE_MODE_Timer;
    NRF_TIMER2->BITMODE   = TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER2->PRESCALER = TIMER_PRESCALERS;

    // Clears the timer, sets it to 0.
    NRF_TIMER2->TASKS_CLEAR = 1;

    // Load the initial values to TIMER2 CC registers.
    NRF_TIMER2->CC[0] = MAX_SAMPLE_LEVELS + duty_cycle;
    NRF_TIMER2->CC[1] = MAX_SAMPLE_LEVELS;

    // CC2 will be set on the first CC1 interrupt.
    NRF_TIMER2->CC[2] = 0;

    // Interrupt setup.
    NRF_TIMER2->INTENSET = (TIMER_INTENSET_COMPARE1_Enabled << TIMER_INTENSET_COMPARE1_Pos);
}


/** @brief Function for initializing the GPIO Tasks/Events peripheral.
 */
static void gpiote_init(void)
{
    // Configure PWM_OUTPUT_PIN_NUMBER as an output.
    nrf_gpio_cfg_output(PWM_OUTPUT_PIN_NUMBER);

    // Configure GPIOTE channel 0 to toggle the PWM pin state
    // @note Only one GPIOTE task can be connected to an output pin.
    nrf_gpiote_task_config(0, PWM_OUTPUT_PIN_NUMBER, \
                           NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);
}


/** @brief Function for initializing the Programmable Peripheral Interconnect peripheral.
 */
static void ppi_init(void)
{
    // Configure PPI channel 0 to toggle PWM_OUTPUT_PIN on every TIMER2 COMPARE[0] match.
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[0];
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];

    // Configure PPI channel 1 to toggle PWM_OUTPUT_PIN on every TIMER2 COMPARE[1] match.
    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[1];
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];

    // Configure PPI channel 1 to toggle PWM_OUTPUT_PIN on every TIMER2 COMPARE[2] match.
    NRF_PPI->CH[2].EEP = (uint32_t)&NRF_TIMER2->EVENTS_COMPARE[2];
    NRF_PPI->CH[2].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];

    // Enable PPI channels 0-2.
    NRF_PPI->CHEN = (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos)
                    | (PPI_CHEN_CH1_Enabled << PPI_CHEN_CH1_Pos)
                    | (PPI_CHEN_CH2_Enabled << PPI_CHEN_CH2_Pos);
}


static void bsp_configuration()
{
    // Start oscillator for app_timer
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // Do nothing.
    }

    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, NULL);
    APP_GPIOTE_INIT(1);

    uint32_t err_code;
    err_code = bsp_init(BSP_INIT_BUTTONS, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), bsp_evt_handler);
    err_code = bsp_buttons_enable( (1 << BUTTON_PREV_ID) | (1 << BUTTON_NEXT_ID) );
    APP_ERROR_CHECK(err_code);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    gpiote_init();
    bsp_configuration();
    ppi_init();
    timer2_init();

    // Enabling constant latency as indicated by PAN 11 "HFCLK: Base current with HFCLK 
    // running is too high" found at Product Anomaly document found at
    // https://www.nordicsemi.com/eng/Products/Bluetooth-R-low-energy/nRF51822/#Downloads
    //
    // @note This example does not go to low power mode therefore constant latency is not needed.
    //       However this setting will ensure correct behaviour when routing TIMER events through 
    //       PPI (shown in this example) and low power mode simultaneously.
    NRF_POWER->TASKS_CONSTLAT = 1;

    // Enable interrupt on Timer 2.
    NVIC_EnableIRQ(TIMER2_IRQn);
    __enable_irq();

    // Workaround for PAN-73: Use of an EVENT from any TIMER module to trigger a TASK in GPIOTE or 
    // RTC using the PPI could fail under certain conditions.
    *(uint32_t *)0x4000AC0C = 1;    
    // Start the timer.
    NRF_TIMER2->TASKS_START = 1;

    while (true)
    {
        // Do nothing.
    }
}


/** @} */
