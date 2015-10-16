#include "printf.h"

#include "app_uart.h"
#include "app_error.h"

void print(const char * str)
{		
		uint32_t index = 0;
		while(str[index] != '\0')
		{
				while(app_uart_put(str[index++]) != NRF_SUCCESS);
		}
}
