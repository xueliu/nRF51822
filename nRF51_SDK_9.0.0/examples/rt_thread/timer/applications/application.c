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

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <shell.h>
#endif

/* time control block */
static struct rt_timer bsp_led_timer1;
static struct rt_timer bsp_led_timer2;
static struct rt_timer bsp_led_timer3;
static struct rt_timer bsp_led_timer4;
static struct rt_timer bsp_led_timer5;

//static rt_timer_t bsp_led_timer1;
//static rt_timer_t bsp_led_timer2;
//static rt_timer_t bsp_led_timer3;
//static rt_timer_t bsp_led_timer4;
//static rt_timer_t bsp_led_timer5;

/* timer1 handler */
static void bsp_led_timerout1(void * parameter)
{
	LEDS_INVERT(BSP_LED_0_MASK);
}

/* timer2 handler */
static void bsp_led_timerout2(void * parameter)
{
	LEDS_INVERT(BSP_LED_1_MASK);
}

/* timer3 handler */
static void bsp_led_timerout3(void * parameter)
{
	LEDS_INVERT(BSP_LED_2_MASK);
}

/* timer4 handler */
static void bsp_led_timerout4(void * parameter)
{
	LEDS_INVERT(BSP_LED_3_MASK);	
}

/* timer5 handler */
static void bsp_led_timerout5(void * parameter)
{
	LEDS_INVERT(BSP_LED_4_MASK);	
}

int rt_application_init(void)
{
	/* initialize timers */
	rt_timer_init(&bsp_led_timer1,
				"timer1",
				bsp_led_timerout1,
				RT_NULL,
				64,
				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

	rt_timer_init(&bsp_led_timer2,
				"timer2",
				bsp_led_timerout2,
				RT_NULL,
				128,
				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

	rt_timer_init(&bsp_led_timer3,
				"timer3",
				bsp_led_timerout3,
				RT_NULL,
				256,
				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

	rt_timer_init(&bsp_led_timer4,
				"timer4",
				bsp_led_timerout4,
				RT_NULL,
				512,
				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

	rt_timer_init(&bsp_led_timer5,
				"timer5",
				bsp_led_timerout5,
				RT_NULL,
				1024,
				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

//	bsp_led_timer1 = rt_timer_create(
//				"timer1",
//				bsp_led_timerout1,
//				RT_NULL,
//				64,
//				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

//	bsp_led_timer2 = rt_timer_create(
//				"timer2",
//				bsp_led_timerout2,
//				RT_NULL,
//				128,
//				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

//	bsp_led_timer3 = rt_timer_create(
//				"timer3",
//				bsp_led_timerout3,
//				RT_NULL,
//				256,
//				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

//	bsp_led_timer4 = rt_timer_create(
//				"timer4",
//				bsp_led_timerout4,
//				RT_NULL,
//				512,
//				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);

//	bsp_led_timer5 = rt_timer_create(
//				"timer5",
//				bsp_led_timerout5,
//				RT_NULL,
//				1024,
//				RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);


	/* start timers */
	rt_timer_start(&bsp_led_timer1);
	rt_timer_start(&bsp_led_timer2);
	rt_timer_start(&bsp_led_timer3);
	rt_timer_start(&bsp_led_timer4);
	rt_timer_start(&bsp_led_timer5);
	
//	rt_timer_start(bsp_led_timer1);
//	rt_timer_start(bsp_led_timer2);
//	rt_timer_start(bsp_led_timer3);
//	rt_timer_start(bsp_led_timer4);
//	rt_timer_start(bsp_led_timer5);
	
    /* Set finsh device */
#ifdef RT_USING_FINSH
    /* initialize finsh */
    finsh_system_init();
#endif
    return 0;
}


/*@}*/
