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

/* points to the thread control blocks */
static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;
static rt_thread_t tid3 = RT_NULL;
static rt_mutex_t mutex = RT_NULL;

/* 线程1入口 */
static void thread1_entry(void* parameter)
{
	/* 先让低优先级线程运行 */
	rt_thread_delay(10);
	
    /* 此时thread3持有mutex，并且thread2等待持有mutex */

    /* 检查thread2与thread3的优先级情况 */
    if (tid2->current_priority != tid3->current_priority)
    {
        /* 优先级不相同，测试失败 */
        tc_stat(TC_STAT_END | TC_STAT_FAILED);
        return;
    }	
}

/* 线程2入口 */
static void thread2_entry(void* parameter)
{
    rt_err_t result;

    /* 先让低优先级线程运行 */
    rt_thread_delay(5);

    while (1)
    {
        /*
         * 试图持有互斥锁，此时thread3持有，应把thread3的优先级提升到thread2相同
         * 的优先级
         */
        result = rt_mutex_take(mutex, RT_WAITING_FOREVER);

        if (result == RT_EOK)
        {
            /* 释放互斥锁 */
            rt_mutex_release(mutex);
        }
    }
}

/* 线程3入口 */
static void thread3_entry(void* parameter)
{
    rt_tick_t tick;
    rt_err_t result;

    while (1)
    {
        result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
        if (result != RT_EOK)
        {
            tc_stat(TC_STAT_END | TC_STAT_FAILED);
        }
        result = rt_mutex_take(mutex, RT_WAITING_FOREVER);
        if (result != RT_EOK)
        {
            tc_stat(TC_STAT_END | TC_STAT_FAILED);
        }

        /* 做一个长时间的循环，总共50个OS Tick */
        tick = rt_tick_get();
        while (rt_tick_get() - tick < 50) ;

        rt_mutex_release(mutex);
        rt_mutex_release(mutex);
    }
}

int mutex_simple_init()
{
	/* creat mutex */
	mutex = rt_mutex_create("mutex", RT_IPC_FLAG_FIFO);
	if (mutex == RT_NULL)
	{
		tc_stat(TC_STAT_END | TC_STAT_FAILED);
		return 0;
	}
	
	/* create thread 1 */
    tid1 = rt_thread_create("t1",
                            thread1_entry, RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY - 1, THREAD_TIMESLICE);
    if (tid1 != RT_NULL)
        rt_thread_startup(tid1);
	else
        tc_stat(TC_STAT_END | TC_STAT_FAILED);
	
    /* create thread 2 */
    tid2 = rt_thread_create("t2",
                            thread2_entry, RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);
    else
        tc_stat(TC_STAT_END | TC_STAT_FAILED);
	
    /* create thread 3 */
    tid3 = rt_thread_create("t3",
                            thread3_entry, RT_NULL,
                            THREAD_STACK_SIZE, THREAD_PRIORITY + 1, THREAD_TIMESLICE);
    if (tid3 != RT_NULL)
        rt_thread_startup(tid3);
    else
        tc_stat(TC_STAT_END | TC_STAT_FAILED);

    return 0;
}

#ifdef RT_USING_TC
static void _tc_cleanup()
{
    /* 调度器上锁，上锁后，将不再切换到其他线程，仅响应中断 */
    rt_enter_critical();

    /* 删除线程 */
    if (tid1 != RT_NULL && tid1->stat != RT_THREAD_CLOSE)
        rt_thread_delete(tid1);
    if (tid2 != RT_NULL && tid2->stat != RT_THREAD_CLOSE)
        rt_thread_delete(tid2);
    if (tid3 != RT_NULL && tid3->stat != RT_THREAD_CLOSE)
        rt_thread_delete(tid3);

    if (mutex != RT_NULL)
    {
        rt_mutex_delete(mutex);
    }

    /* 调度器解锁 */
    rt_exit_critical();

    /* 设置TestCase状态 */
    tc_done(TC_STAT_PASSED);
}

int _tc_mutex_simple()
{
    /* 设置TestCase清理回调函数 */
    tc_cleanup(_tc_cleanup);
    mutex_simple_init();

    /* 返回TestCase运行的最长时间 */
    return 100;
}
/* 输出函数命令到finsh shell中 */
FINSH_FUNCTION_EXPORT(_tc_mutex_simple, sime mutex example);
#else
/* 用户应用入口 */
int rt_application_init(void)
{
	mutex_simple_init();
	
    /* Set finsh device */
#ifdef RT_USING_FINSH
    /* initialize finsh */
    finsh_system_init();
#endif
    return 0;
}
#endif

/*@}*/
