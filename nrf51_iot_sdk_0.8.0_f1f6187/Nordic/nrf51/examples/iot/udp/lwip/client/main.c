/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
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
 *
 * @defgroup iot_sdk_app_lwip_udp_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 * @brief This file contains the source code for LwIP UDP Client sample application.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "app_timer.h"
#include "app_button.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ble_6lowpan.h"
#include "mem_manager.h"
#include "app_trace.h"
#include "lwip/init.h"
#include "lwip/inet6.h"
#include "lwip/ip6.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
/*lint -save -e607 Suppress warning 607 "Parameter p of macro found within string" */
#include "lwip/udp.h"
/*lint -restore */
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"

#define DEVICE_NAME                         "LwIPUDPClient"                                         /**< Device name used in BLE undirected advertisement. */

/** Remote UDP Port Address on which data is transmitted.
 *  Modify m_remote_addr according to your setup.
 *  The address provided below is a place holder.  */
static const ip6_addr_t                     m_remote_addr =
{
    .addr =
    {HTONL(0x20040000),
    0x00000000,
    HTONL(0x02AABBFF),
    HTONL(0xFECCDDEE)}
};

#define APP_TIMER_PRESCALER                 NRF51_DRIVER_TIMER_PRESCALER                            /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS                1                                                       /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE             1
#define LWIP_SYS_TIMER_INTERVAL             APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER)              /**< Interval for timer used as trigger to send. */

#define ADVERTISING_LED                     BSP_LED_0_MASK                                          /**< Is on when device is advertising. */
#define CONNECTED_LED                       BSP_LED_1_MASK                                          /**< Is on when device is connected. */
#define DISPLAY_LED_0                       BSP_LED_0_MASK                                          /**< LED used for displaying mod 4 of data payload received on UDP port. */
#define DISPLAY_LED_1                       BSP_LED_1_MASK                                          /**< LED used for displaying mod 4 of data payload received on UDP port. */
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

#define APP_ADV_TIMEOUT                     0                                                       /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL                MSEC_TO_UNITS(100, UNIT_0_625_MS)                       /**< The advertising interval. This value can vary between 100ms to 10.24s). */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define UDP_SERVER_PORT                     9000                                                    /**< Remote UDP port number on which data is sent. */
#define UDP_CLIENT_PORT                     9001                                                    /**< Local UDP port used to send ping requests and receive responses. */
#define UDP_DATA_SIZE                       8                                                       /**< UDP Data size sent on remote. */


eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ble_gap_adv_params_t                 m_adv_params;                                           /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t                       m_sys_timer_id;                                         /**< System Timer used to service LwIP timers periodically. */
static struct udp_pcb                     * mp_udp_port;                                            /**< UDP Port to listen on. */

static uint32_t                             m_sequence_number;                                      /**< Counter used for sequencing data packets transmitted. */

/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    //Halt the application and notify of error using the LEDs.
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n",error_code, line_num, p_file_name);
    LEDS_ON(ALL_APP_LED);
    for(;;)
    {
    }

    // @note: In case on assert, it is desired to only recover and reset, uncomment the line below.
    //NVIC_SystemReset();
}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by this application.
 */
static void leds_init(void)
{
    // Configure application LED pins.
    LEDS_CONFIGURE(ALL_APP_LED);

    // Turn off all LED on initialization.
    LEDS_OFF(ALL_APP_LED);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t                err_code;
    ble_advdata_t           advdata;
    uint8_t                 flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    ble_gap_conn_sec_mode_t sec_mode;
    ble_gap_addr_t          my_addr;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_address_get(&my_addr);
    APP_ERROR_CHECK(err_code);

    my_addr.addr[5]   = 0x00;
    my_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;

    err_code = sd_ble_gap_address_set(&my_addr);
    APP_ERROR_CHECK(err_code);

    IPV6_EUI64_CREATE_FROM_EUI48(eui64_local_iid.identifier,
                                 my_addr.addr,
                                 my_addr.addr_type);

    ble_uuid_t adv_uuids[] =
    {
        {BLE_UUID_IPSP_SERVICE,         BLE_UUID_TYPE_BLE}
    };

    //Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.flags                   = flags;
    advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = adv_uuids;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);

    //Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.p_peer_addr = NULL;                             // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = APP_ADV_ADV_INTERVAL;
    m_adv_params.timeout     = APP_ADV_TIMEOUT;
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);

    LEDS_ON(ADVERTISING_LED);
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            APPL_LOG ("[APPL]: Connected.\r\n");
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            APPL_LOG ("[APPL]: Disconnected.\r\n");
            advertising_start();
            break;
        default:
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    ble_ipsp_evt_handler(p_ble_evt);
    on_ble_evt(p_ble_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, false);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Callback registered with UDP module to handle incoming data on the port.
 *
 * @details Callback registered with UDP module to handle incoming data on the port.
 *
 * @param[in] p_arg          Any arguments registered by application on the port.
 * @param[in] p_pcb          Identifies the UDP port on which data is received.
 * @param[in] p_buffer       Buffer containing data and its length.
 * @param[in] p_remote_addr  Address of the remote side from which data was received.
 * @param[in] remote_port    Remote port number used when sending the data to the application port.
 */
void udp_recv_data_handler(void              * p_arg,
                           struct udp_pcb    * p_pcb,
                           struct pbuf       * p_buffer,
                           ip6_addr_t        * p_remote_addr,
                           u16_t               remote_port)
{
    APPL_LOG ("[APPL]: >> UDP Data Rx on Port 0x%08X\r\n", remote_port);
    APPL_DUMP (p_buffer->payload,p_buffer->len);

    uint8_t *p_data = p_buffer->payload;

    if (p_buffer->len == UDP_DATA_SIZE)
    {
        uint32_t sequnce_number = 0;

        sequnce_number  = ((p_data[0] << 24) & 0xFF000000);
        sequnce_number |= ((p_data[1] << 16) & 0x00FF0000);
        sequnce_number |= ((p_data[2] << 8)  & 0x0000FF00);
        sequnce_number |= (p_data[3]         & 0x000000FF);

        LEDS_OFF(DISPLAY_LED_0 | DISPLAY_LED_1);

        if (sequnce_number & 0x00000001)
        {
            LEDS_ON(DISPLAY_LED_0);
        }
        if (sequnce_number & 0x00000002)
        {
            LEDS_ON(DISPLAY_LED_1);
        }

        if (sequnce_number != m_sequence_number)
        {
            APPL_LOG ("[APPL]: Mismatch in sequence number.\r\n");
        }
    }
    else
    {
        APPL_LOG ("[APPL]: UDP data received in incorrect format.\r\n");
    }

    APPL_LOG ("[APPL]: << UDP Data Rx on Port 0x%08X\r\n", remote_port);
}


/**@brief UDP Port Set-Up.
 *
 * @details Sets up UDP Port to listen on.
 */
static void udp_port_setup(void)
{
    ip6_addr_t any_addr;
    ip6_addr_set_any(&any_addr);

    mp_udp_port = udp_new_ip6();

    if (mp_udp_port != NULL)
    {
        err_t err = udp_bind_ip6(mp_udp_port, &any_addr, UDP_CLIENT_PORT);
        APP_ERROR_CHECK(err);

        udp_recv_ip6(mp_udp_port,udp_recv_data_handler, NULL);
    }
    else
    {
        ASSERT(0);
    }
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack and its driver.
 */
static void ip_stack_init(void)
{
    uint32_t err_code = nrf51_sdk_mem_init();
    APP_ERROR_CHECK(err_code);

    //Initialize LwIP stack.
    lwip_init();

    //Initialize LwIP stack driver.
    err_code = nrf51_driver_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Sends UDP data in Request format described in description above.
 *
 * @details Sends UDP data in Request of size 8 in format described in description above.
 */
static void udp_data_send(void)
{
    struct pbuf * p_send_buf;
    uint8_t     * p_udp_data;

    APPL_LOG ("[APPL]: >> UDP Data Tx\r\n");
    m_sequence_number++;

    p_send_buf = pbuf_alloc (PBUF_TRANSPORT, UDP_DATA_SIZE, PBUF_RAM);
    ASSERT(p_send_buf != NULL);

    p_send_buf->len = UDP_DATA_SIZE;
    p_udp_data = p_send_buf->payload;

    p_udp_data[0] = (uint8_t )((m_sequence_number >> 24) & 0x000000FF);
    p_udp_data[1] = (uint8_t )((m_sequence_number >> 16) & 0x000000FF);
    p_udp_data[2] = (uint8_t )((m_sequence_number >> 8) & 0x000000FF);
    p_udp_data[3] = (uint8_t )(m_sequence_number & 0x000000FF);

    p_udp_data[4] = 'P';
    p_udp_data[5] = 'i';
    p_udp_data[6] = 'n';
    p_udp_data[7] = 'g';

    //Send UDP Data packet.
    err_t err = udp_sendto_ip6(mp_udp_port, p_send_buf, &m_remote_addr, UDP_SERVER_PORT);
    if (err != ERR_OK)
    {
        APPL_LOG ("[APPL]: Failed to send UDP packet, reason %d\r\n", err);
        m_sequence_number--;
    }

    APPL_DUMP(p_send_buf->payload,p_send_buf->len);
    APPL_LOG ("[APPL]: << UDP Data Tx, %d len %d\r\n", err, p_send_buf->len);

    UNUSED_VARIABLE(pbuf_free(p_send_buf));

}


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending UDP packets on the port.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void system_timer_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    static int count = 0;
    sys_check_timeouts();
    count++;
    if (count == 10)
    {
        count = 9;
        udp_data_send();
    }
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timers.
    err_code = app_timer_create(&m_sys_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                system_timer_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf51_driver_interface_up(void)
{
    uint32_t err_code;

    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    err_code = app_timer_start(m_sys_timer_id, LWIP_SYS_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

    LEDS_OFF(ADVERTISING_LED);
    LEDS_ON(CONNECTED_LED);
}


/**@brief Function to handle interface down event. */
void nrf51_driver_interface_down(void)
{
    uint32_t err_code;

    APPL_LOG ("[APPL]: IPv6 interface down.\r\n");

    err_code = app_timer_stop(m_sys_timer_id);
    APP_ERROR_CHECK(err_code);

    LEDS_ON(ADVERTISING_LED);
    LEDS_OFF(CONNECTED_LED);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    //Initialize.
    app_trace_init();
    leds_init();
    timers_init();
    ble_stack_init();
    advertising_init();
    ip_stack_init ();

    udp_port_setup();

    //Start execution.
    advertising_start();

    //Enter main loop.
    for (;;)
    {
        //Sleep waiting for an application event.
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
