#include <stdbool.h>
#include "boards.h"
#include "nrf_drv_gpiote.h"
#include "app_error.h"

#include "app_timer.h"
#include "nrf_drv_clock.h"

// Pins for LED's and buttons.
// The diodes on the DK are connected with the cathodes to the GPIO pin, so
// clearing a pin will light the LED and setting the pin will turn of the LED.
#define LED_1_PIN                       BSP_LED_0     // LED 1 on the nRF51-DK or nRF52-DK
#define LED_2_PIN                       BSP_LED_1     // LED 3 on the nRF51-DK or nRF52-DK
#define BUTTON_1_PIN                    BSP_BUTTON_0  // Button 1 on the nRF51-DK or nRF52-DK
#define BUTTON_2_PIN                    BSP_BUTTON_1  // Button 2 on the nRF51-DK or nRF52-DK

// General application timer settings.
#define APP_TIMER_PRESCALER             16    // Value of the RTC1 PRESCALER register.
#define APP_TIMER_MAX_TIMERS            2     // Maximum number of timers in this application.
#define APP_TIMER_OP_QUEUE_SIZE         3     // Size of timer operation queues.

static app_timer_id_t                   m_led_a_timer_id;

// Button event handler.
void gpiote_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	ret_code_t err_code;
	
    switch (pin)
    {
    case BUTTON_1_PIN:
//        nrf_drv_gpiote_out_clear(LED_1_PIN);
        err_code = app_timer_start(m_led_a_timer_id, APP_TIMER_TICKS(200, APP_TIMER_PRESCALER), NULL);
        APP_ERROR_CHECK(err_code);
        break;
    case BUTTON_2_PIN:
//        nrf_drv_gpiote_out_set(LED_1_PIN);
		err_code = app_timer_stop(m_led_a_timer_id);
        APP_ERROR_CHECK(err_code);
        break;
    default:
        break;
    }
}


// Function for configuring GPIO.
static void gpio_config()
{
    ret_code_t err_code;

    // Initialze driver.
    err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);

    // Configure 3 output pins for LED's.
    nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
    err_code = nrf_drv_gpiote_out_init(LED_1_PIN, &out_config);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_gpiote_out_init(LED_2_PIN, &out_config);
    APP_ERROR_CHECK(err_code);

    // Set output pins (this will turn off the LED's).
    nrf_drv_gpiote_out_set(LED_1_PIN);
    nrf_drv_gpiote_out_set(LED_2_PIN);

    // Make a configuration for input pints. This is suitable for both pins in this example.
    nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    in_config.pull = NRF_GPIO_PIN_PULLUP;

    // Configure input pins for buttons, with separate event handlers for each button.
    err_code = nrf_drv_gpiote_in_init(BUTTON_1_PIN, &in_config, gpiote_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_gpiote_in_init(BUTTON_2_PIN, &in_config, gpiote_event_handler);
    APP_ERROR_CHECK(err_code);

    // Enable input pins for buttons.
    nrf_drv_gpiote_in_event_enable(BUTTON_1_PIN, true);
    nrf_drv_gpiote_in_event_enable(BUTTON_2_PIN, true);
}

// Function starting the internal LFCLK oscillator.
// This is needed by RTC1 which is used by the application timer
// (When SoftDevice is enabled the LFCLK is always running and this is not needed).

static void lfclk_request(void)
{
	uint32_t err_code = nrf_drv_clock_init(NULL);
	APP_ERROR_CHECK(err_code);
	nrf_drv_clock_lfclk_request();
}

// Timeout handler for the repeated timer
static void timer_a_handler(void * p_context)
{
    nrf_drv_gpiote_out_toggle(LED_1_PIN);
}

// Create timers
static void create_timers()
{   
    uint32_t err_code;

    // Create timers
    err_code = app_timer_create(&m_led_a_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                timer_a_handler);
    APP_ERROR_CHECK(err_code);
}

// Main function.
int main(void)
{
    // Configure GPIO's.
    gpio_config();

	// Request LF clock.
    lfclk_request();
	
	 // Initialize the application timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

	// Create application timers.
	create_timers();
	
    // Main loop.
    while (true)
    {
        // Wait for interrupt.
        __WFI();
    }
}
