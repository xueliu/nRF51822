/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
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
 *
 * @defgroup iot_sdk_app_tcp_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 *
 * @brief This file contains the source code for LwIP TCP Client sample application.
 */

#include <stdio.h>
#include <stdint.h>
#include "bsp.h"
#include "app_uart.h"
#include "app_error.h"
#include "app_trace.h"
#include "nordic_common.h"
#include "app_timer.h"
#include "iot_timer.h"

#define APP_TIMER_PRESCALER                 31                                                      /**< Prescaler value for timer module to get a tick of about 1ms. */
#define APP_TIMER_MAX_TIMERS                1                                                       /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE             3                                                       /**< Size of timer operation queues. */

#define IOT_TIMER_CLIENT_ONE_CB_INTERVAL    100                                                     /**< Interval in milliseconds between callbacks for one of the IoT Timer clients. */
#define IOT_TIMER_CLIENT_TWO_CB_INTERVAL    200                                                     /**< Interval in milliseconds between callbacks for one of the IoT Timer clients. */
#define IOT_TIMER_CLIENT_THREE_CB_INTERVAL  400                                                     /**< Interval in milliseconds between callbacks for one of the IoT Timer clients. */
#define IOT_TIMER_CLIENT_FOUR_CB_INTERVAL   800                                                     /**< Interval in milliseconds between callbacks for one of the IoT Timer clients. */

#define DISPLAY_LED_0                       BSP_LED_0_MASK                                          /**< LED used for displaying IoT Timer callbacks. */
#define DISPLAY_LED_1                       BSP_LED_1_MASK                                          /**< LED used for displaying IoT Timer callbacks. */
#define DISPLAY_LED_2                       BSP_LED_2_MASK                                          /**< LED used for displaying IoT Timer callbacks. */
#define DISPLAY_LED_3                       BSP_LED_3_MASK                                          /**< LED used for displaying IoT Timer callbacks. */
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

static app_timer_id_t                       m_iot_timer_tick_src_id;                                /**< App timer instance used to update the IoT timer wall clock. */

/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    //Halt the application and notify of error using the LEDs.
    printf("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n", error_code, line_num, p_file_name);

    LEDS_ON(ALL_APP_LED);
    for(;;)
    {
        //Do nothing.
    }

    // @note: In case on assert, it is desired to only recover and reset, uncomment the line below.
    //NVIC_SystemReset();
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by this application.
 */
static void leds_init(void)
{
    // Configure application LED pins.
    LEDS_CONFIGURE(ALL_APP_LED);

    // Turn off all LED on initialization.
    LEDS_OFF(ALL_APP_LED);
}


/**@brief Function for updating the wall clock of the IoT Timer module.
 */
static void iot_timer_tick_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    uint32_t err_code = iot_timer_update();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the App Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, NULL);

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting the low frequency clock.
 */
static void low_feq_clock_start(void)
{
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        //No implementation needed.
    }

    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
}


/**@brief Timer callback used for periodic changing of LED state.
 */
static void iot_timer_client_one_callback(iot_timer_time_in_ms_t elapsed_time)
{
    UNUSED_PARAMETER(elapsed_time);
    LEDS_INVERT(DISPLAY_LED_0);
    printf("---- %4d ms\r\n", IOT_TIMER_CLIENT_ONE_CB_INTERVAL);
}


/**@brief Timer callback used for periodic changing of LED state.
 */
static void iot_timer_client_two_callback(iot_timer_time_in_ms_t elapsed_time)
{
    UNUSED_PARAMETER(elapsed_time);
    LEDS_INVERT(DISPLAY_LED_1);
    printf("----------- %4d ms\r\n", IOT_TIMER_CLIENT_TWO_CB_INTERVAL);
}


/**@brief Timer callback used for periodic changing of LED state.
 */
static void iot_timer_client_three_callback(iot_timer_time_in_ms_t elapsed_time)
{
    UNUSED_PARAMETER(elapsed_time);
    LEDS_INVERT(DISPLAY_LED_2);
    printf("------------------ %4d ms\r\n", IOT_TIMER_CLIENT_THREE_CB_INTERVAL);
}


/**@brief Timer callback used for periodic changing of LED state.
 */
static void iot_timer_client_four_callback(iot_timer_time_in_ms_t elapsed_time)
{
    UNUSED_PARAMETER(elapsed_time);
    LEDS_INVERT(DISPLAY_LED_3);
    nrf_gpio_pin_toggle(13);
    printf("------------------------- %4d ms\r\n", IOT_TIMER_CLIENT_FOUR_CB_INTERVAL);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    //Initialize.
    app_trace_init();
    leds_init();
    low_feq_clock_start();
    timers_init();

    const iot_timer_client_t list_of_clients[] =
    {
        {iot_timer_client_one_callback,   IOT_TIMER_CLIENT_ONE_CB_INTERVAL},
        {iot_timer_client_two_callback,   IOT_TIMER_CLIENT_TWO_CB_INTERVAL},
        {iot_timer_client_three_callback, IOT_TIMER_CLIENT_THREE_CB_INTERVAL},
        {iot_timer_client_four_callback,  IOT_TIMER_CLIENT_FOUR_CB_INTERVAL},
    };

    //The list of IoT Timer clients is declared as a constant.
    const iot_timer_clients_list_t iot_timer_clients =
    {
        (sizeof(list_of_clients) / sizeof(iot_timer_client_t)),
        &(list_of_clients[0]),
    };

    //Passing the list of clients to the IoT Timer module.
    err_code = iot_timer_client_list_set(&iot_timer_clients);
    APP_ERROR_CHECK(err_code);

    //Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id, \
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER), \
                               NULL);
    APP_ERROR_CHECK(err_code);

    //Enter main loop.
    for (;;)
    {
        //Do nothing.
    }
}

/** @} */
