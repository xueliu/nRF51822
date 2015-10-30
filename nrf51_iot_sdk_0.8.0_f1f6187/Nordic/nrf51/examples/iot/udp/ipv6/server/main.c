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
 * @defgroup iot_sdk_app_nrf_udp_server main.c
 * @{
 * @ingroup iot_sdk_app_nrf
 * @brief This file contains the source code for nRF UDP Server sample application.
 *
 * This application is intended to work with the Nordic UDP Client sample application.
 * Check TX_PORT and RX_PORT for port configuration.
 *
 * An Echo Response is sent in reply to an Echo Requests from any address, but
 * reception of Echo Requests from from the Client are indicated by the LEDs.
 *
 * If a UDP packet is received from the Client, it is immediately sent back regardless
 * of its contents without any modification.
 *
 * The operation of the application is reflected by two LEDs on the board:
 *
 * +-----------+-----------+
 * |   LED 1   |   LED 2   |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    Off    |       Device advertising as BLE peripheral.       |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     | Blinking  |     BLE link established, IPv6 interface down.    |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    Off    |      BLE link established, IPv6 interface up.     |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     | Two rapid |            Echo Request received and              |
 * |           |  flashes  |              Echo Response is sent.               |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    On     |   If a UDP packet is received from the Client,    |
 * |           |           | it is immediately sent back and LED 1 is toggled. |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    On     |       Assertion failure in the application.       |
 * +-----------+-----------+---------------------------------------------------+
 *
 * The MSC below illustrates the data flow.
 *
 *       +------------------+                                              +------------------+
 *       | UDP Client       |                                              | UDP Server       |
 *       |                  |                                              |(this application)|
 *       +------------------+                                              +------------------+
 *                |                                                                 |
 *                |                                                                 |
 *                |                                                         Listening on RX_PORT
 *                |                                                                 |
 *                |                                                                 |
 *                |[ICMPv6 Echo Request]                                            |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                          [ICMPv6 Echo Response] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *                |[UDP Payload : [Sequence Number 1 | Data ]]                      |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                                    Sequence number logged on UART
 *                |                                                         if trace is enabled.
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 1 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *  Sequence number is incremented                                                  |
 *                |                                                                 |
 *                |[UDP Payload : [Sequence Number 2 | Data ]]                      |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *                |                                                    Sequence number logged on UART
 *                |                                                         if trace is enabled.
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 2 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 *  Sequence number is incremented                                                  |
 *                |                 .                                               |
 *                |                 .                                               |
 *                |                 .                                               |
 *                |                                                                 |
 *                -                                                                 -
 */

#include <stdbool.h>
#include <stdint.h>
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

#define DEVICE_NAME                   "UDP6_Server_Node"                                            /**< Device name used in BLE undirected advertisement. */

#define LED_ONE                       BSP_LED_0_MASK
#define LED_TWO                       BSP_LED_1_MASK

#define UDP_PORT                      0x1717                                                        /**< Port for transmission of UDP packets. */

#define APP_TIMER_PRESCALER           0                                                             /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS          2                                                             /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE       5                                                             /**< Size of timer operation queues. */

#define APP_ADV_TIMEOUT               0                                                             /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL          MSEC_TO_UNITS(333, UNIT_0_625_MS)                             /**< The advertising interval. This value can vary between 100ms to 10.24s). */
#define LED_BLINK_INTERVAL            APP_TIMER_TICKS(250, APP_TIMER_PRESCALER)                     /**< LED blinking interval. */

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
    LEDS_INACTIVE = 0,
    LEDS_BLE_ADVERTISING,
    LEDS_IPV6_IF_DOWN,
    LEDS_TX_ECHO_RESPONSE,
    LEDS_TX_UDP_PACKET
} display_state_t;

static ble_gap_addr_t         m_local_ble_addr;                                                     /**< Local GAP address. */
static ble_gap_adv_params_t   m_adv_params;                                                         /**< Parameters to be passed to the stack when starting advertising. */
static udp6_socket_t          m_udp_socket;                                                         /**< UDP socket used for reception and transmission. */
static app_timer_id_t         m_led_blink_timer;                                                    /**< Timer instance used for controlling board LEDs. */
static app_timer_id_t         m_echo_ind_timer;                                                     /**< Timer instance used for controlling board LEDs. */
static bool                   m_udp_tx_occured = false;                                             /**< Indicates UDP transmission for controlling board LEDs. */
static display_state_t        m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */


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
    LEDS_ON((LED_ONE | LED_TWO));
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
    // Configure leds.
    LEDS_CONFIGURE((LED_ONE | LED_TWO));

    // Turn leds off.
    LEDS_OFF((LED_ONE | LED_TWO));
}


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void blink_timeout_handler(void * p_context)
{
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
            break;
        }
        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            break;
        }
        case LEDS_TX_ECHO_RESPONSE:
        {
            LEDS_ON(LED_ONE);
            LEDS_OFF(LED_TWO);
            break;
        }
        case LEDS_TX_UDP_PACKET:
        {
            LEDS_ON(LED_TWO);
            if (LED_IS_ON(LED_ONE) == 0)
            {
                if (m_udp_tx_occured)
                {
                    // If UDP transmission stops, LED_ONE should stop blinking in off state.
                    LEDS_ON(LED_ONE);
                    m_udp_tx_occured = false;
                }
            }
            else
            {
                LEDS_OFF(LED_ONE);
            }

            break;
        }
        default:
        {
            break;
        }
    }
}


/**@brief Timer callback used for indicating the transmission of an echo response to the client
 *        node with the board LEDs.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void response_ind_tout_handler(void * p_context)
{
    static uint8_t tout_counter=0;
    ++tout_counter;
    LEDS_INVERT(LED_TWO);
    if (tout_counter == 3)
    {
        tout_counter = 0;
        return;
    }
    uint32_t err_code = app_timer_start(m_echo_ind_timer, (LED_BLINK_INTERVAL / 3), NULL);
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

    // Create timer to control board LEDs.
    err_code = app_timer_create(&m_led_blink_timer, APP_TIMER_MODE_REPEATED, \
                                blink_timeout_handler);
    APP_ERROR_CHECK(err_code);

    // Create timer to control board LEDs when an echo response to the client is transmitted.
    err_code = app_timer_create(&m_echo_ind_timer, APP_TIMER_MODE_SINGLE_SHOT, \
                                response_ind_tout_handler);
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


/**@brief Callback handler to receive data on UDP port.
 *
 * @param[in]   p_socket         Socket identifier.
 * @param[in]   p_ip_header      IPv6 header containing source and destination addresses.
 * @param[in]   p_udp_header     UDP header identifying local and remote endpoints.
 * @param[in]   process_result   Result of data reception, there could be possible errors like
 *                               invalid checksum etc.
 * @param[in]   iot_pbuffer_t    Packet buffer containing the received data packet.
 *
 * @retval      NRF_SUCCESS      Indicates received data was handled successfully, else an error
 *                               code indicating reason for failure.
 */
uint32_t rx_udp_port_app_handler(const udp6_socket_t * p_socket,
                                 const ipv6_header_t * p_ip_header,
                                 const udp6_header_t * p_udp_header,
                                 uint32_t              process_result,
                                 iot_pbuffer_t       * p_rx_packet)
{
    // APPL_LOG("[APPL]: Got UDP6 data on socket 0x%08lx\r\n", p_socket->socket_id);
    // APPL_LOG("[APPL]: Source IPv6 Address: ");
    // APPL_ADDR(p_ip_header->srcaddr);

    APP_ERROR_CHECK(process_result);

    // Print PORTs
    // APPL_LOG("[APPL]: UDP Destination port: %lx\r\n", HTONS(p_udp_header->destport));
    // APPL_LOG("[APPL]: UDP Source port: %lx\r\n",      HTONS(p_udp_header->srcport));

    // UDP packet received from the Client node.
    APPL_LOG("[APPL]: Received UDP packet sequence number: %ld\r\n", \
             uint32_decode(&p_rx_packet->p_payload[0]));

    iot_pbuffer_alloc_param_t   pbuff_param;
    iot_pbuffer_t             * p_tx_buffer;

    pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
    pbuff_param.type   = UDP6_PACKET_TYPE;
    pbuff_param.length = p_rx_packet->length;

    // Allocate packet buffer.
    uint32_t err_code = iot_pbuffer_allocate(&pbuff_param, &p_tx_buffer);
    APP_ERROR_CHECK(err_code);

    memcpy(p_tx_buffer->p_payload, p_rx_packet->p_payload, p_rx_packet->length);

    // Send back the received packet payload without any modification.
    err_code = udp6_socket_sendto(&m_udp_socket, &p_ip_header->srcaddr, HTONS(UDP_PORT), p_tx_buffer);
    APP_ERROR_CHECK(err_code);

    m_display_state  = LEDS_TX_UDP_PACKET;
    m_udp_tx_occured = true;

    return NRF_SUCCESS;
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
    uint32_t err_code;
    APPL_LOG("[APPL]: Got ICMP6 Application Handler Event on interface 0x%p\r\n", p_interface);

    APPL_LOG("[APPL]: Source IPv6 Address: ");
    APPL_ADDR(p_ip_header->srcaddr);
    APPL_LOG("[APPL]: Destination IPv6 Address: ");
    APPL_ADDR(p_ip_header->destaddr);
    APPL_LOG("[APPL]: Process result = 0x%08lx\r\n", process_result);

    switch(p_icmp_header->type)
    {
        case ICMP6_TYPE_DESTINATION_UNREACHABLE:
            APPL_LOG("[APPL]: ICMP6 Message Type = Destination Unreachable Error\r\n");
            break;
        case ICMP6_TYPE_PACKET_TOO_LONG:
            APPL_LOG("[APPL]: ICMP6 Message Type = Packet Too Long Error\r\n");
            break;
        case ICMP6_TYPE_TIME_EXCEED:
            APPL_LOG("[APPL]: ICMP6 Message Type = Time Exceed Error\r\n");
            break;
        case ICMP6_TYPE_PARAMETER_PROBLEM:
            APPL_LOG("[APPL]: ICMP6 Message Type = Parameter Problem Error\r\n");
            break;
        case ICMP6_TYPE_ECHO_REQUEST:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Request\r\n");
            m_display_state = LEDS_TX_ECHO_RESPONSE;
            LEDS_OFF(LED_TWO);
            err_code = app_timer_start(m_echo_ind_timer, (LED_BLINK_INTERVAL / 3), NULL);
            APP_ERROR_CHECK(err_code);

            break;
        case ICMP6_TYPE_ECHO_REPLY:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");
            break;
        case ICMP6_TYPE_ROUTER_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Solicitation\r\n");
            break;
        case ICMP6_TYPE_ROUTER_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Router Advertisement\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_SOLICITATION:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Solicitation\r\n");
            break;
        case ICMP6_TYPE_NEIGHBOR_ADVERTISEMENT:
            APPL_LOG("[APPL]: ICMP6 Message Type = Neighbor Advertisement\r\n");
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
    uint32_t err_code;

    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch(p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
            APPL_LOG("[APPL]: New interface added!\r\n");

            err_code = udp6_socket_allocate(&m_udp_socket);
            APP_ERROR_CHECK(err_code);

            err_code = udp6_socket_bind(&m_udp_socket, IPV6_ADDR_ANY, HTONS(UDP_PORT));
            APP_ERROR_CHECK(err_code);

            err_code = udp6_socket_recv(&m_udp_socket, rx_udp_port_app_handler);
            APP_ERROR_CHECK(err_code);

            m_display_state  = LEDS_TX_ECHO_RESPONSE;

            break;
        case IPV6_EVT_INTERFACE_DELETE:
            APPL_LOG("[APPL]: Interface removed!\r\n");

            err_code = udp6_socket_free(&m_udp_socket);
            APP_ERROR_CHECK(err_code);

            m_display_state  = LEDS_IPV6_IF_DOWN;

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
    err_code = app_timer_start(m_led_blink_timer, LED_BLINK_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
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

    err_code = ipv6_transport_init();
    APP_ERROR_CHECK(err_code);

    // Initialize IP Stack.
    ip_stack_init();

    APPL_LOG("\r\n");
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
