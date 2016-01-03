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
 
#include "leddriver.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "boards.h"


static const uint32_t m_led_id_mapping_table[LED_ID_MAX] = {LED_0, LED_1};    /**< LED hardware pin ID mapping table. */ 


void leddriver_init(void)
{
    // Configure led pins as outputs.
    nrf_gpio_range_cfg_output(LED_START, LED_STOP);
}


void leddriver_led_turn_on(leddriver_led_id_t led_id)
{
    nrf_gpio_pin_set(m_led_id_mapping_table[led_id]);    
}


void leddriver_led_turn_off(leddriver_led_id_t led_id)
{
    nrf_gpio_pin_clear(m_led_id_mapping_table[led_id]);    
}


void leddriver_led_toggle(leddriver_led_id_t led_id)
{
    nrf_gpio_pin_toggle(m_led_id_mapping_table[led_id]);
}
