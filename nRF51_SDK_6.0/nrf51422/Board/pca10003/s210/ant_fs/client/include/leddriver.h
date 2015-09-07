/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
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

/**@file
 * @brief The LED driver interface.
 * @defgroup ant_fs_client_main ANT-FS client device simulator
 * @{
 * @ingroup nrf_ant_fs_client
 *
 * @brief The ANT-FS client device simulator.
 *
 */

#ifndef LEDDRIVER_H__
#define LEDDRIVER_H__

// Led IDs. 
typedef enum 
{
    LED_ID_0,   /**< Led 0 ID. */ 
    LED_ID_1,   /**< Led 1 ID. */  
    LED_ID_MAX
} leddriver_led_id_t;
 
/**@brief Function for configuring all the GPIOs needed by the driver.
 */
void leddriver_init(void);

/**@brief Function for turning on a single LED.
 *
 * @param[in] led_id ID of the led to be turned on.
 */
void leddriver_led_turn_on(leddriver_led_id_t led_id);

/**@brief Function for turning off a single LED.
 *
 * @param[in] led_id ID of the led to be turned off.
 */
void leddriver_led_turn_off(leddriver_led_id_t led_id);

/**@brief Function for toggling a single LED.
 *
 * @param[in] led_id ID of the led to be toggled.
 */
void leddriver_led_toggle(leddriver_led_id_t led_id);

#endif // LEDDRIVER_H__

/**
 *@}
 **/
