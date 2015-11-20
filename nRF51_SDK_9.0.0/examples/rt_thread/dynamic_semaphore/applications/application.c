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
#include "tc_comm.h"

#include "nrf_gpio.h"
#include "boards.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h>
#endif

static rt_thread_t tid = RT_NULL;

static rt_sem_t sem = RT_NULL;

/* thread entry */
static void thread_entry(void * parameter)
{
	rt_err_t result;
	rt_tick_t tick;
	
	/* get current OS tick */
	tick = rt_tick_get();
	
	/* Try to get the sem, return timeout than 10 OS tick */
	result = rt_sem_take(sem, 10);
	if (result == -RT_ETIMEOUT)
	{
		if(rt_tick_get() - tick != 10)
		{
			LEDS_ON(BSP_LED_1_MASK);
			tc_done(TC_STAT_FAILED);
			rt_sem_delete(sem);
			return;
		}
		LEDS_ON(BSP_LED_0_MASK);
		rt_kprintf("take semaphore timeout\n");
	}
	else
	{
		tc_done(TC_STAT_FAILED);
		rt_sem_delete(sem);
	}
	
	/* release sem once */
	rt_sem_release(sem);
	
	result = rt_sem_take(sem, RT_WAITING_FOREVER);
	if (result != RT_EOK)
	{
		tc_done(TC_STAT_FAILED);
		rt_sem_delete(sem);
		return;
	}
	rt_kprintf("take semaphore\n");
	tc_done(TC_STAT_PASSED);
	rt_sem_delete(sem);
}

int semaphore_dynamic_init()
{
    /* 创建一个信号量，初始值是0 */
    sem = rt_sem_create("sem", 0, RT_IPC_FLAG_FIFO);
    if (sem == RT_NULL)
    {
        tc_stat(TC_STAT_END | TC_STAT_FAILED);
        return 0;
    }

    /* 创建线程 */
    tid = rt_thread_create("thread",
                           thread_entry, RT_NULL, /* 线程入口是thread_entry, 入口参数是RT_NULL */
                           THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    else
        tc_stat(TC_STAT_END | TC_STAT_FAILED);

    return 0;
}

int rt_application_init(void)
{
	LEDS_OFF(BSP_LED_0_MASK);
	LEDS_OFF(BSP_LED_1_MASK);
	
	semaphore_dynamic_init();
	
    /* Set finsh device */
#ifdef RT_USING_FINSH
    /* initialize finsh */
    finsh_system_init();
#endif
    return 0;
}

#ifdef RT_USING_TC
static void _tc_cleanup()
{
	rt_enter_critical();
	
	if(tid != RT_NULL && tid->stat != RT_THREAD_CLOSE)
	{
		rt_thread_delete(tid);
		
		/* delete semaphore */
		rt_sem_delete(sem);
	}
	
	rt_exit_critical();
	
	tc_done(TC_STAT_PASSED);
}

int _tc_semaphore_dynamic()
{
	tc_cleanup(_tc_cleanup);
	semaphore_dynamic_init();
	
	return 100;
}

FINSH_FUNCTION_EXPORT(_tc_semaphore_dynamic, a dynamic semaphore example);
#else
int rt_application_init()
{
	semaphore_dynamic_init();
	
	return 0;
}
#endif

/*@}*/
