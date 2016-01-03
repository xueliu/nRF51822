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
  
#include <stdio.h>
#include <stdint.h>
#include "app_uart.h"
#include "nordic_common.h"

// Structure for stdio printf() to hold the file handle. 
struct __FILE 
{ 
    int handle; 
};

FILE __stdout;
FILE __stdin;


int fputc(int ch, FILE * p_file) 
{
    const uint32_t err_code = app_uart_put((uint8_t)ch);
    UNUSED_VARIABLE(err_code);
    return 0;
}

