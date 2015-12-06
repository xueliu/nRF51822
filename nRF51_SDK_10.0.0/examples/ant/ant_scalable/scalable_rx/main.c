/*
This software is subject to the license described in the license.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Dynastream Innovations Inc. 2015
All rights reserved.
*/

#include <stdint.h>
#include "nrf.h"
#include "app_error.h"
#include "boards.h"
#include "softdevice_handler.h"
#include "ant_stack_config.h"
#include "ant_scalable_rx.h"


/**@brief Function for application main entry. Does not return.
 */
int main(void)
{
    uint32_t err_code;

    // Setup LEDs
    LEDS_CONFIGURE(LEDS_MASK);
    ant_scaleable_display_num_tracking_channels();

    // Setup SoftDevice and events handler
    err_code = softdevice_ant_evt_handler_set(ant_scaleable_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = softdevice_handler_init(NRF_CLOCK_LFCLKSRC, NULL, 0, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = ant_stack_static_config();
    APP_ERROR_CHECK(err_code);

    // Setup and open channels as an RX Slave
    ant_scaleable_channel_rx_broadcast_setup();

    // Enter main loop
    for (;;)
    {
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}
