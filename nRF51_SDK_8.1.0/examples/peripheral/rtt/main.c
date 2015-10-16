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
 * @defgroup rtt_example_main main.c
 * @{
 * @ingroup rtt_example
 * @brief rtt Example Application main file.
 *
 * This file contains the source code for a sample application for RTT
 * 
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"

#include "SEGGER_RTT.h"

/**
 * @brief Function for main application entry.
 */
int main(void)
{
		int variable = 0;
		while(1) {
//				SEGGER_RTT_WriteString(0, "Hello World!\n");
				SEGGER_RTT_printf(0, "variable value: %d\n", variable++);
				nrf_delay_ms(1000);
		}
}


/** @} */
