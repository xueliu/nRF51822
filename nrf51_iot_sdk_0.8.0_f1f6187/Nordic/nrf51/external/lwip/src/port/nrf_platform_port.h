/* Copyright (c)  2014 Nordic Semiconductor. All Rights Reserved.
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

/** @file nrf51_driver.h
 *
 * @brief Implements lwip stack driver on nrf51.
 *
 * @details Implements lwip stack driver on nrf51.
 */

#ifndef NRF51_DRIVER_H__
#define NRF51_DRIVER_H__

/**@brief Prescaler value for timer module to get a tick of about 1ms.
 *
 * @note Applications using lwIP and this driver, must use this value of prescaler when
 *       initializing the timer module.
 */
#define NRF51_DRIVER_TIMER_PRESCALER    31

/**@biref Initializes the driver for LwIP stack. */
uint32_t nrf51_driver_init(void);

/**@biref API assumed to be implemented by the application to handle interface up event. */
void nrf51_driver_interface_up(void);

/**@biref API assumed to be implemented by the application to handle interface down event. */
void nrf51_driver_interface_down(void);

#endif //NRF51_DRIVER_H__
