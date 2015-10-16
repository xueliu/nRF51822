#ifndef __PRINTF_H_
#define __PRINTF_H_

#include "app_uart.h"

#define PRINTF_INIT APP_UART_FIFO_INIT 

void print(const char * str);

#endif
