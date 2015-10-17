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
#include "nrf_drv_twi.h"
#include "app_util_platform.h"
#include "mpu6050.h"

#include "SEGGER_RTT.h"

/**
 * @brief Function for main application entry.
 */
int main(void)
{
	uint32_t err_code;
	int16_t tem1[3];
//		uint8_t tx_data[] = {'a', 'b', 'c', 'd', 'e'};
		
	const nrf_drv_twi_t 		twi 		= NRF_DRV_TWI_INSTANCE(0);
	const nrf_drv_twi_config_t 	twi_config 	= NRF_DRV_TWI_DEFAULT_CONFIG(0);
	err_code = nrf_drv_twi_init(&twi, &twi_config, NULL);
	APP_ERROR_CHECK(err_code);
	nrf_drv_twi_enable(&twi);
	
//	err_code = nrf_drv_twi_tx(&twi, MPU6050_DEFAULT_ADDRESS, tx_data, sizeof(tx_data), false);
//	APP_ERROR_CHECK(err_code);
	
	mpu6050_init(&twi);

	while(true)
	{
		MPU6050_ReadAcc(&twi, &tem1[0], &tem1[1] , &tem1[2] );
		nrf_delay_ms(1000);
		printf("ACC:  %d	%d	%d	\n\r",tem1[0],tem1[1],tem1[2]);
	}
}


/** @} */
