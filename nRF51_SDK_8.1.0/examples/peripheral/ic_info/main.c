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
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "nrf_ic_info.h"
#include <string.h>

/**
 * @brief Function for application main entry.
 */
int main(void)
{
	nrf_ic_info_t ic_info;
	
	/* fill ic_info with null characters */
	memset (&ic_info, '\0', sizeof (ic_info));
  
	nrf_ic_info_get(&ic_info);
	
	printf("Hello\n\r");
}


/** @} */
