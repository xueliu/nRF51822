/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
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
 * @defgroup iot_sdk_app_nrf_udp_client main.c
 * @{
 * @ingroup iot_sdk_app_nrf
 * @brief This file contains the source code for nRF UDP Client sample application.
 *
 */

#include <stdbool.h>
#include <stdint.h>
/*lint -save -e43 -e1504 */
#include <time.h>
/*lint -restore*/
#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "mem_manager.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "udp_api.h"
#include "app_trace.h"
#include "app_timer.h"
#include "app_button.h"
#include "app_util.h"
#include "dns6_api.h"
#include "iot_timer.h"
#include "sntp_client.h"

#define DEVICE_NAME                   "SNTP_Client"                                                 /**< Device name used in BLE undirected advertisement. */

#define LED_ONE                       BSP_LED_0_MASK
#define LED_TWO                       BSP_LED_1_MASK
#define LED_THREE                     BSP_LED_2_MASK
#define LED_FOUR                      BSP_LED_3_MASK
#define ALL_APP_LED                  (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                      BSP_LED_2_MASK | BSP_LED_3_MASK)                              /**< Define used for simultaneous operation of all application LEDs. */

#define QUERY_ONE_BUTTON_PIN_NO       BSP_BUTTON_0                                                  /**< Button used to query an NTP server. */
#define QUERY_TWO_BUTTON_PIN_NO       BSP_BUTTON_1                                                  /**< Button used to query an NTP server. */
#define PRINT_TIME_BUTTON_PIN_NO      BSP_BUTTON_2                                                  /**< Button used to print local time. */

#define LOCAL_SNTP_UDP_PORT           0x1818                                                        /**< UDP port for SNTP client module. */
#define SERVER_SNTP_UDP_PORT          0x007B                                                        /**< UDP Port number of SNTP Server. */

#define LOCAL_DNS_UDP_PORT            0x1717                                                        /**< UDP port for DNS client module. */
#define SERVER_DNS_UDP_PORT           0x0035                                                        /**< UDP Port number of DNS Server. */
#define APP_DNS_SERVER_ADDR           {{0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00,  \
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88}}            /**< IPv6 address of DNS Server. */

#define APP_TIMER_PRESCALER           0                                                             /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS          2                                                             /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE       5                                                             /**< Size of timer operation queues. */

#define APP_ADV_TIMEOUT               0                                                             /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL          MSEC_TO_UNITS(333, UNIT_0_625_MS)                             /**< The advertising interval. This value can vary between 100ms to 10.24s). */
#define LED_BLINK_INTERVAL_MS         300                                                           /**< LED blinking interval. */
                                        
#define DEAD_BEEF                     0xDEADBEEF                                                    /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME           128                                                           /**< Max length of filename to copy for the debug error handler. */

#define APP_DISABLE_LOGS              0                                                             /**< Disable debug trace in the application. */

#if (APP_DISABLE_LOGS == 0)

#define APPL_LOG  app_trace_log
#define APPL_DUMP app_trace_dump

#else // APP_DISABLE_LOGS

#define APPL_LOG(...)
#define APPL_DUMP(...)

#endif // APP_DISABLE_LOGS

#define APPL_ADDR(addr) APPL_LOG("%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\r\n", \
                                (addr).u8[0],(addr).u8[1],(addr).u8[2],(addr).u8[3],                            \
                                (addr).u8[4],(addr).u8[5],(addr).u8[6],(addr).u8[7],                            \
                                (addr).u8[8],(addr).u8[9],(addr).u8[10],(addr).u8[11],                          \
                                (addr).u8[12],(addr).u8[13],(addr).u8[14],(addr).u8[15])

typedef enum
{
    APP_STATE_IPV6_IF_DOWN = 1,
    APP_STATE_IPV6_IF_UP,
    APP_STATE_HOSTNAME_RESOLUTION,
    APP_STATE_NTP_QUERY
} tx_app_state_t;

typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_BLE_ADVERTISING,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP,
    LEDS_HOSTNAME_RESOLUTION,
    LEDS_NTP_QUERY
} display_state_t;


static ble_gap_addr_t         m_local_ble_addr;                                                     /**< Local GAP address. */
static ble_gap_adv_params_t   m_adv_params;                                                         /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t         m_iot_timer_tick_src_id;                                              /**< App timer instance used to update the IoT timer wall clock. */
static bool                   m_is_ipv6_if_up = false;                                              /**< Variable representing the state of the IPv6 interface. */
static tx_app_state_t         m_node_state = APP_STATE_IPV6_IF_DOWN;                                /**< Application inner state. */
static display_state_t        m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */
static const char             m_ntp_server_one_hostname[] = "ntp6a.rollernet.us";                   /**< Host name to resolve and target for NTP query. */
static const char             m_ntp_server_two_hostname[] = "ntp6b.rollernet.us";                   /**< Host name to resolve and target for NTP query. */
static uint16_t               m_ntp_local_port = LOCAL_SNTP_UDP_PORT;                               /**< Local port number for NTP queries. */
static ipv6_addr_t            m_ntp_server_address;                                                 /**< IPv6 address of a server hostname. */
static uint16_t               m_ntp_server_port = SERVER_SNTP_UDP_PORT;                             /**< Port number of the NTP service on the server. */


/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    LEDS_ON(ALL_APP_LED);
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n",error_code, line_num, p_file_name);

    // Variables used to retain function parameter values when debugging.
    static volatile uint8_t  s_file_name[MAX_LENGTH_FILENAME];
    static volatile uint16_t s_line_num;
    static volatile uint32_t s_error_code;

    strncpy((char *)s_file_name, (const char *)p_file_name, MAX_LENGTH_FILENAME - 1);
    s_file_name[MAX_LENGTH_FILENAME - 1] = '\0';
    s_line_num                           = line_num;
    s_error_code                         = error_code;
    UNUSED_VARIABLE(s_file_name);
    UNUSED_VARIABLE(s_line_num);
    UNUSED_VARIABLE(s_error_code);

    // This call can be used for debug purposes during application development.
    // @note CAUTION: Activating this code will write the stack to flash on an error.
    //                This function should NOT be used in a final product.
    //                It is intended STRICTLY for development/debugging purposes.
    //                The flash write will happen EVEN if the radio is active, thus interrupting
    //                any communication.
    //                Use with care. Un-comment the line below to use.
    // ble_debug_assert_handler(error_code, line_num, p_file_name);

    // On assert, the system can only recover on reset.
    //NVIC_SystemReset();

    for(;;)
    {
        // Infinite loop.
    }
}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
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


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

    switch (m_display_state)
    {
        case LEDS_INACTIVE:
        {
            LEDS_OFF((LED_ONE | LED_TWO));
            break;
        }
        case LEDS_BLE_ADVERTISING:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_OFF(LED_TWO);
            LEDS_OFF(LED_THREE | LED_FOUR);
            break;
        }
        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            LEDS_OFF(LED_THREE | LED_FOUR);
            break;
        }
        case LEDS_IPV6_IF_UP:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            LEDS_OFF(LED_THREE | LED_FOUR);
            break;
        }
        case LEDS_HOSTNAME_RESOLUTION:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            LEDS_INVERT(LED_THREE);
            LEDS_OFF(LED_FOUR);
            break;
        }
        case LEDS_NTP_QUERY:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            LEDS_INVERT(LED_FOUR);
            LEDS_OFF(LED_THREE);
            break;
        }
        default:
        {
            break;
        }
    }
}


/**@brief DNS6 module event handler.
 *
 * @details Callback registered with the DNS6 module to receive asynchronous events from
 *          the module for registering query.
 */
static void app_dns_handler(uint32_t      process_result,
                            const char  * p_hostname,
                            ipv6_addr_t * p_addr,
                            uint16_t      addr_count)
{
    uint32_t index;

    if (m_node_state != APP_STATE_HOSTNAME_RESOLUTION)
    {
        // Exit if it's not in resolving state.
        return;
    }

    APPL_LOG("[APPL]: DNS Response for hostname: %s, with %d IPv6 addresses and status 0x%08lX\r\n",
    p_hostname, addr_count, process_result);

    if(process_result == NRF_SUCCESS)
    {
        for (index = 0; index < addr_count; index++)
        {
            APPL_LOG("[APPL]: [%ld] IPv6 Address: ", index);
            APPL_ADDR(p_addr[index]);

            // Store only first given address, but print all of them.
            if(index == 0)
            {
                memcpy(m_ntp_server_address.u8, p_addr[0].u8, IPV6_ADDR_SIZE);
            }
        }
        // Change application state.
        m_node_state = APP_STATE_NTP_QUERY;
        m_display_state = LEDS_NTP_QUERY;

        // Send SNTP query.
        uint32_t err_code = sntp_client_server_query(&m_ntp_server_address, \
                                                     m_ntp_server_port,     \
                                                     true);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        APPL_LOG("[APPL]: DNS query failed.\r\n");
        // Start application state machine from beginning.
        m_node_state = APP_STATE_IPV6_IF_UP;
        m_display_state = LEDS_IPV6_IF_UP;
    }
}


/**@brief Function for handling button events.
 *
 * @param[in]   pin_no         The pin number of the button pressed.
 * @param[in]   button_action  The action performed on button.
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    uint32_t err_code;

    if (button_action == APP_BUTTON_PUSH)
    {
        if (pin_no == PRINT_TIME_BUTTON_PIN_NO)
        {
            time_t locally_stored_time;
            err_code = sntp_client_local_time_get(&locally_stored_time);
            APP_ERROR_CHECK(err_code);

            APPL_LOG("\r\n");
            APPL_LOG("        -------------------------------------\r\n");
            APPL_LOG("            UTC  %s\r", ctime(&locally_stored_time));
            APPL_LOG("        -------------------------------------\r\n\r\n");
        }
        else
        {
            // Check if IPv6 interface is UP.
            if (m_is_ipv6_if_up == false)
            {
                APPL_LOG("[APPL]: The IPv6 interface is down. \r\n\r\n");
                return;
            }

            switch (pin_no)
            {
                case QUERY_ONE_BUTTON_PIN_NO:
                {
                    if (m_node_state != APP_STATE_IPV6_IF_UP)
                    {
                        APPL_LOG("[APPL]: Wait until previous query is finished... \r\n");
                    }
                    else
                    {
                        m_node_state = APP_STATE_HOSTNAME_RESOLUTION;
                        APPL_LOG("[APPL]: Sending DNS query to: %s\r\n", m_ntp_server_one_hostname);

                        err_code = dns6_query(m_ntp_server_one_hostname, app_dns_handler);
                        APP_ERROR_CHECK(err_code);

                        m_display_state = LEDS_HOSTNAME_RESOLUTION;
                    }

                    break;
                }
                case QUERY_TWO_BUTTON_PIN_NO:
                {
                    if (m_node_state != APP_STATE_IPV6_IF_UP)
                    {
                        APPL_LOG("[APPL]: Wait until previous query is finished... \r\n");
                    }
                    else
                    {
                        m_node_state = APP_STATE_HOSTNAME_RESOLUTION;
                        APPL_LOG("[APPL]: Sending DNS query to: %s\r\n", m_ntp_server_two_hostname);

                        err_code = dns6_query(m_ntp_server_two_hostname, app_dns_handler);
                        APP_ERROR_CHECK(err_code);

                        m_display_state = LEDS_HOSTNAME_RESOLUTION;
                    }

                    break;
                }
                default:
                  break;
            }
        }
    }
}


/**@brief Function for the Button initialization.
 *
 * @details Initializes all Buttons used by this application.
 */
static void buttons_init(void)
{
    uint32_t err_code;

    static app_button_cfg_t buttons[] =
    {
        {QUERY_ONE_BUTTON_PIN_NO,   false, BUTTON_PULL, button_event_handler},
        {QUERY_TWO_BUTTON_PIN_NO,   false, BUTTON_PULL, button_event_handler},
        {PRINT_TIME_BUTTON_PIN_NO,  false, BUTTON_PULL, button_event_handler}
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for updating the wall clock of the IoT Timer module.
 */
static void iot_timer_tick_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    uint32_t err_code = iot_timer_update();
    APP_ERROR_CHECK(err_code);
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

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,   \
                                APP_TIMER_MODE_REPEATED,    \
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t        err_code;
    ble_advdata_t   advdata;
    uint8_t         flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_address_get(&m_local_ble_addr);
    APP_ERROR_CHECK(err_code);

    m_local_ble_addr.addr[5]   = 0x00;
    m_local_ble_addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;

    err_code = sd_ble_gap_address_set(&m_local_ble_addr);
    APP_ERROR_CHECK(err_code);

    ble_uuid_t adv_uuids[] =
    {
        {BLE_UUID_IPSP_SERVICE, BLE_UUID_TYPE_BLE}
    };

    //Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.flags                   = flags;
    advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = adv_uuids;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.p_peer_addr = NULL;                             // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = APP_ADV_ADV_INTERVAL;
    m_adv_params.timeout     = APP_ADV_TIMEOUT;
}


/**@brief ICMP6 module event handler.
 *
 * @details Callback registered with the ICMP6 module to receive asynchronous events from
 *          the module, if the ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION constant is not zero
 *          or the ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION constant is not zero.
 */
uint32_t icmp6_handler(iot_interface_t  * p_interface,
                       ipv6_header_t    * p_ip_header,
                       icmp6_header_t   * p_icmp_header,
                       uint32_t           process_result,
                       iot_pbuffer_t    * p_rx_packet)
{
    #define ICMP_LOG(...)
    #define ICMP_ADDR(...)
    ICMP_LOG("[APPL]: Got ICMP6 Application Handler Event on interface 0x%p\r\n", p_interface);

    ICMP_LOG("[APPL]: Source IPv6 Address: ");
    ICMP_ADDR(p_ip_header->srcaddr);
    ICMP_LOG("[APPL]: Destination IPv6 Address: ");
    ICMP_ADDR(p_ip_header->destaddr);
    ICMP_LOG("[APPL]: Process result = 0x%08lx\r\n", process_result);

    switch(p_icmp_header->type)
    {
        case ICMP6_TYPE_DESTINATION_UNREACHABLE:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Destination Unreachable Error\r\n");
            break;
        case ICMP6_TYPE_PACKET_TOO_LONG:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Packet Too Long Error\r\n");
            break;
        case ICMP6_TYPE_TIME_EXCEED:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Time Exceed Error\r\n");
            break;
        case ICMP6_TYPE_PARAMETER_PROBLEM:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Parameter Problem Error\r\n");
            break;
        case ICMP6_TYPE_ECHO_REQUEST:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Echo Request\r\n");
            break;
        case ICMP6_TYPE_ECHO_REPLY:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");
            break;
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Router Solicitation\r\n");
            break;
        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Router Advertisement\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Neighbor Solicitation\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            ICMP_LOG("[APPL]: ICMP6 Message Type = Neighbor Advertisement\r\n");
            break;
        default:
            break;
    }

    return NRF_SUCCESS;
}


/**@brief IP stack event handler.
 *
 * @details Callback registered with the ICMP6 to receive asynchronous events from the module.
 */
void ip_app_handler(iot_interface_t * p_interface, ipv6_event_t * p_event)
{
    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch(p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
            APPL_LOG("[APPL]: New interface added!\r\n");

            m_is_ipv6_if_up = true;
            m_node_state    = APP_STATE_IPV6_IF_UP;
            m_display_state = LEDS_IPV6_IF_UP;
            break;
        case IPV6_EVT_INTERFACE_DELETE:
            APPL_LOG("[APPL]: Interface removed!\r\n");

            m_is_ipv6_if_up = false;
            m_node_state    = APP_STATE_IPV6_IF_DOWN;
            m_display_state = LEDS_IPV6_IF_DOWN;
            break;
        case IPV6_EVT_INTERFACE_RX_DATA:
            APPL_LOG("[APPL]: Got unsupported protocol data!\r\n");
            break;
        default:
            //Unknown event. Should not happen.
            break;
    }
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack.
 */
static void ip_stack_init(void)
{
   uint32_t    err_code;
   eui64_t     eui64_addr;
   ipv6_init_t init_param;

   IPV6_EUI64_CREATE_FROM_EUI48(eui64_addr.identifier,
                                m_local_ble_addr.addr,
                                m_local_ble_addr.addr_type);

   init_param.p_eui64       = &eui64_addr;
   init_param.event_handler = ip_app_handler;

   err_code = ipv6_init(&init_param);
   APP_ERROR_CHECK(err_code);

   err_code = icmp6_receive_register(icmp6_handler);
   APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);

    APPL_LOG("[APPL]: Advertising.\r\n");

    m_display_state = LEDS_BLE_ADVERTISING;
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

            m_display_state = LEDS_IPV6_IF_DOWN;
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            APPL_LOG ("[APPL]: Disconnected.\r\n");

            m_display_state = LEDS_INACTIVE;
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


/**@brief Function for initializing the transport of IPv6. */
uint32_t ipv6_transport_init(void)
{
    ble_stack_init();
    advertising_init();

    return NRF_SUCCESS;
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack.
 */
static void dns_client_init(void)
{
    uint32_t    err_code;

    // Configure DNS client and server parameters.
    dns6_init_t dns_init_param =
    {
        .local_src_port  = LOCAL_DNS_UDP_PORT,
        .dns_server      =
        {
            .port = SERVER_DNS_UDP_PORT,
            .addr = APP_DNS_SERVER_ADDR
        }
    };

    err_code = dns6_init(&dns_init_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling SNTP client module events.
 *
 * @details Event handler procedure registered with the SNTP client module for receiving
 *          asynchronous callbacks.
 *
 */
void sntp_evt_handler(const ipv6_addr_t * ntp_srv_addr, \
                      uint16_t ntp_srv_udp_port,        \
                      uint32_t process_result,          \
                      sntp_client_cb_param_t callback_parameter)
{
    if ((process_result == NRF_SUCCESS) || (process_result == NTP_SERVER_KOD_PACKET_RECEIVED))
    {
        APPL_LOG("\r\n");
        APPL_LOG("[APPL]:    NTP response from: ");
        APPL_ADDR(*ntp_srv_addr);
        APPL_LOG("[APPL]:             UDP port: %5d\r\n", ntp_srv_udp_port);

        switch (process_result)
        {
            case NRF_SUCCESS:
            {
                APPL_LOG("        -------------------------------------\r\n");
                APPL_LOG("            UTC  %s\r", ctime(&callback_parameter.time_from_server));
                APPL_LOG("        -------------------------------------\r\n\r\n");
                break;
            }
            case NTP_SERVER_KOD_PACKET_RECEIVED:
            {
                APPL_LOG("[APPL]:    Kiss-o'-Death Packet received.\r\n");
                APPL_LOG("[APPL]:    Kiss code: %.*s\r\n\r\n", \
                         KISS_CODE_LEN, (char *)&callback_parameter.callback_data);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else
    {
        APPL_LOG("\r\n");
        APPL_LOG("[APPL]: SNTP query failed. \r\n");

        switch (process_result)
        {
            case (NRF_ERROR_INVALID_DATA | IOT_NTP_ERR_BASE):
            {
                APPL_LOG("[APPL]: Malformed UPD packet received. \r\n\r\n");
                break;
            }
            case NTP_SERVER_BAD_RESPONSE:
            {
                APPL_LOG("[APPL]: NTP header sanity check failed. \r\n\r\n");
                break;
            }
            case NTP_SERVER_UNREACHABLE:
            {
                APPL_LOG("[APPL]: No response from NTP server. \r\n\r\n");
                break;
            }
            default:
            {
                break;
            }
        }
    }

    m_node_state = APP_STATE_IPV6_IF_UP;
    m_display_state = LEDS_IPV6_IF_UP;
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack.
 */
static void sntp_c_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
    static sntp_client_init_param_t sntp_client_init_param;

    // Set local UDP port.
    sntp_client_init_param.local_udp_port  = m_ntp_local_port;
    // Set event handler callback procedure.
    sntp_client_init_param.app_evt_handler = sntp_evt_handler;

    err_code = sntp_client_init(&sntp_client_init_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler,       LED_BLINK_INTERVAL_MS},
        {dns6_timeout_process,        SEC_TO_MILLISEC(DNS6_RETRANSMISSION_INTERVAL)},
        {sntp_client_timeout_process, SEC_TO_MILLISEC(SNTP_RETRANSMISSION_INTERVAL)}
    };

    // The list of IoT Timer clients is declared as a constant.
    static const iot_timer_clients_list_t iot_timer_clients =
    {
        (sizeof(list_of_clients) / sizeof(iot_timer_client_t)),
        &(list_of_clients[0]),
    };

    // Passing the list of clients to the IoT Timer module.
    err_code = iot_timer_client_list_set(&iot_timer_clients);
    APP_ERROR_CHECK(err_code);

    // Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id, \
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER), \
                               NULL);
    APP_ERROR_CHECK(err_code);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;

    // Initialize.
    app_trace_init();
    leds_init();
    timers_init();
    buttons_init();

    err_code = ipv6_transport_init();
    APP_ERROR_CHECK(err_code);

    // Initialize IP Stack.
    ip_stack_init();
    dns_client_init();

    // Initialize SNTP client.
    sntp_c_init();
    // Initialize IoT timer module.
    iot_timer_init();

    APPL_LOG("\r\n\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution.
    advertising_start();

    // Enter main loop.
    for (;;)
    {
        /* Sleep waiting for an application event. */
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
