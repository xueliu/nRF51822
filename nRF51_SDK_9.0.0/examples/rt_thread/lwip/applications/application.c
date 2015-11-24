/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2015, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2015-03-01     Yangfs       the first version
 * 2015-03-27     Bernard      code cleanup.
 */

/**
 * @addtogroup NRF51822
 */
/*@{*/

#include <rtthread.h>

#include "nrf_gpio.h"
#include "boards.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h>
#endif

#ifdef RT_USING_LWIP
#include "virtual_ethernet.h"
#include <netif/ethernetif.h>
extern int lwip_system_init(void);
#endif

/* thread phase init */
void rt_init_thread_entry(void *parameter)
{
#ifdef RT_USING_LWIP
    /* register Ethernet interface device */
    rt_hw_virtual_ethernet_init();

    /* initialize lwip stack */
	/* register ethernetif device */
	eth_system_device_init();

	/* initialize lwip system */
	lwip_system_init();
	rt_kprintf("TCP/IP initialized!\n");
#endif
	
    /* Set finsh device */
#ifdef RT_USING_FINSH
    /* initialize finsh */
    finsh_system_init();
#endif
}

int rt_application_init(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("init",
    		rt_init_thread_entry, RT_NULL,
    		2048, RT_THREAD_PRIORITY_MAX/3, 20);
    if (tid != RT_NULL) rt_thread_startup(tid);

    return 0;
}


/*@}*/
