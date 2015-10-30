/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
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
 * This application is intended to work with the Nordic UDP Server sample application.
 * The Server is indentified with its address, SERVER_IPV6_ADDRESS. Check TX_PORT and RX_PORT
 * for port configuration.
 *
 * The application sends Echo Requests to the Server to determine whether it's reachable.
 * Once an Echo Response is received, the application starts to send UDP packets to the peer.
 * The payload of the UDP packets is 20 bytes long and has the following format:
 *  +----------------------------+---------------------------+
 *  |  Packet sequence number    |  16 bytes of random data  |
 *  | in uint32 format (4 bytes) |                           |
 *  +----------------------------+---------------------------+
 *
 * The Server is expected to send back the UDP packets without any modification.
 * Each transmitted packet is stored until it's received back or until the buffer is full.
 *
 * The buffer of transmitted packets is of PACKET_BUFFER_LEN long. If it gets full,
 * the transmission of UDP packets stop and the buffer is emptied. The packet sequence
 * number is set to zero. Echo Requests are sent to the Server to determine whether it
 * is reachable. When an Echo Response is received, transmission of UDP packets starts again.
 *
 * The operation of the application is reflected by two LEDs on the board:
 *
 * +-----------+-----------+
 * |   LED 1   |   LED 2   |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    Off    |       Device advertising as BLE peripheral.       |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    Off    |     BLE link established, IPv6 interface down.    |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     | Blinking  |         IPv6 interface up, Echo Requests          |
 * |           |           |              are sent to the Server.              |
 * +-----------+-----------+---------------------------------------------------+
 * | Blinking  |    On     |        UDP packets are sent to the Server.        |
 * +-----------+-----------+---------------------------------------------------+
 * |    On     |    On     |       Assertion failure in the application.       |
 * +-----------+-----------+---------------------------------------------------+
 *
 * The MSC below illustrates the data flow. The UDP transmissions are triggered by a timer,
 * reception is asynchronous.
 *
 *       +------------------+                                              +------------------+
 *       | UDP Client       |                                              | UDP Server       |
 *       |(this application)|                                              |                  |
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
 *  Transmitted UDP payload stored                                                  |
 *                |                                                                 |
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 1 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 * Recieved UDP payload is matched                                                  |
 *    against stored payload(s)                                                     |
 *                |                                                                 |
 *  Sequence number is incremented                                                  |
 *                |                                                                 |
 *                |[UDP Payload : [Sequence Number 2 | Data ]]                      |
 *                |---------------------------------------------------------------->|
 *                |                                                                 |
 *  Transmitted UDP payload stored                                                  |
 *                |                                                                 |
 *                |                                                                 |
 *                |                     [UDP Payload : [Sequence Number 2 | Data ]] |
 *                |<----------------------------------------------------------------|
 *                |                                                                 |
 * Recieved UDP payload is matched                                                  |
 *    against stored payload(s)                                                     |
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
#include "app_util.h"

#define DEVICE_NAME                   "UDP6_Client_Node"                                            /**< Device name used in BLE undirected advertisement. */

#define LED_ONE                       BSP_LED_0_MASK
#define LED_TWO                       BSP_LED_1_MASK

/** Modify SERVER_IPV6_ADDRESS according to your setup.
 *  The address provided below is a place holder.  */
#define SERVER_IPV6_ADDRESS           0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                      0x02, 0xAA, 0xAA, 0xFF, 0xFE, 0xAA, 0xAA, 0xAA                /**< IPv6 address of the server node. */

#define UDP_PORT                      0x1717                                                        /**< Port for transmission of UDP packets. */

#define APP_TIMER_PRESCALER           0                                                             /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS          2                                                             /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE       5                                                             /**< Size of timer operation queues. */

#define APP_ADV_TIMEOUT               0                                                             /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL          MSEC_TO_UNITS(333, UNIT_0_625_MS)                             /**< The advertising interval. This value can vary between 100ms to 10.24s). */
#define APP_PING_INTERVAL             APP_TIMER_TICKS(750, APP_TIMER_PRESCALER)                     /**< Interval between sending Echo Requests to the peer. */
#define TX_INTERVAL                   APP_TIMER_TICKS(125, APP_TIMER_PRESCALER)                     /**< Interval between sending UDP6 packets to the peer. */
#define LED_BLINK_INTERVAL            APP_TIMER_TICKS(250, APP_TIMER_PRESCALER)                     /**< LED blinking interval. */

#define PACKET_BUFFER_LEN             5                                                             /**< Number of stored transmitted UDP6 packets. */
#define TEST_PACKET_NUM_LEN           4                                                             /**< Length of UDP6 packet sequence number, in bytes. */
#define TEST_PACKET_DATA_LEN          16                                                            /**< Length of test data in UDP6 packet payload, in bytes. */
#define TEST_PACKET_PAYLOAD_LEN       TEST_PACKET_NUM_LEN + TEST_PACKET_DATA_LEN                    /**< Full length of UDP6 packet payload. */

#define INVALID_PACKET_SEQ_NUMBER     0                                                             /**< Zero not used as a packet sequence number. */

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

typedef struct
{
    uint8_t packet_seq_num[TEST_PACKET_NUM_LEN];
    uint8_t packet_data[TEST_PACKET_DATA_LEN];
} test_packet_payload_t;

typedef enum
{
    APP_STATE_IPV6_IF_DOWN = 1,
    APP_STATE_IPV6_IF_UP,
    APP_STATE_PEER_REACHABLE
} tx_app_state_t;

typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_BLE_ADVERTISING,
    LEDS_IPV6_IF_DOWN,
    LEDS_TX_ECHO_REQUEST,
    LEDS_TX_UDP_PACKET
} display_state_t;

static ble_gap_addr_t         m_local_ble_addr;                                                     /**< Local GAP address. */
static ble_gap_adv_params_t   m_adv_params;                                                         /**< Parameters to be passed to the stack when starting advertising. */
static udp6_socket_t          m_udp_socket;                                                         /**< UDP socket used for reception and transmission. */
static app_timer_id_t         m_tx_node_timer;                                                      /**< Timer instance used for executing transmissions. */
static app_timer_id_t         m_led_blink_timer;                                                    /**< Timer instance used for controlling board LEDs. */
static uint32_t               m_pkt_seq_num = 0;                                                    /**< Transmitted UDP sequence number in payload. */
static const uint32_t         m_invalid_pkt_seq_num = INVALID_PACKET_SEQ_NUMBER;                    /**< Variable storing constant invalid packet sequence number. */
static tx_app_state_t         m_node_state = APP_STATE_IPV6_IF_DOWN;                                /**< Application inner state. */
static display_state_t        m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */
static uint8_t                m_packet_buffer[PACKET_BUFFER_LEN][TEST_PACKET_PAYLOAD_LEN];          /**< Buffer for storing transmitted packets. */


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


/**@brief Function for getting the index number of a stored packet or the index number of an empty
 *        slot in the buffer.
 *
 * @details Returns the index number of a stored packet with the provided packet sequence number.
 *          Use with INVALID_PACKET_SEQ_NUMBER as input to get the index number of the first
 *          empty slot in the buffer.
 *
 * @param[out] p_index_number         Index number of the first packet with matching sequence
 *                                    number.
 * @param[in]  pkt_seq_num            Packet sequence number.
 *
 * @retval     NRF_SUCCESS            Matching slot found in the buffer.
 * @retval     NRF_ERROR_NOT_FOUND    No match found in the buffer.
 */
static uint32_t get_packet_buffer_index(uint32_t * p_index_number, uint32_t * pkt_seq_num)
{
    uint32_t i;
    for (i = 0; i < PACKET_BUFFER_LEN; i++)
    {
        uint32_t rx_sequence_num = uint32_decode(&m_packet_buffer[i][0]);
        if (rx_sequence_num == *pkt_seq_num)
        {
            *p_index_number = i;
            return NRF_SUCCESS;
        }
    }
    *p_index_number = 0;
    return NRF_ERROR_NOT_FOUND;
}


/**@brief Timer callback used for transmitting Echo Request and UDP6 packets depending on
 *        application state.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void tx_timeout_handler(void * p_context)
{
    uint32_t err_code;

    switch (m_node_state)
    {
        case APP_STATE_IPV6_IF_DOWN:
        {
            return;
        }
        case APP_STATE_IPV6_IF_UP:
        {
            APPL_LOG("[APPL]: Ping remote node. \r\n");

            iot_pbuffer_alloc_param_t   pbuff_param;
            iot_pbuffer_t             * p_buffer;

            pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
            pbuff_param.type   = ICMP6_PACKET_TYPE;
            pbuff_param.length = ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET + 10;

            // Allocate packet buffer.
            err_code = iot_pbuffer_allocate(&pbuff_param, &p_buffer);
            APP_ERROR_CHECK(err_code);

            ipv6_addr_t dest_ipv6_addr;
            memcpy(&dest_ipv6_addr.u8[0], (uint8_t[]){SERVER_IPV6_ADDRESS}, IPV6_ADDR_SIZE);

            iot_interface_t * p_interface;
            ipv6_addr_t       src_ipv6_addr;
            err_code = ipv6_address_find_best_match(&p_interface,
                                                    &src_ipv6_addr,
                                                    &dest_ipv6_addr);
            APP_ERROR_CHECK(err_code);

            memset(p_buffer->p_payload + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET, 'A', 10);

            // Send Echo Request to peer.
            err_code = icmp6_echo_request(p_interface, &src_ipv6_addr, &dest_ipv6_addr, p_buffer);
            APP_ERROR_CHECK(err_code);

            err_code = app_timer_start(m_tx_node_timer, APP_PING_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);

            break;
        }
        case APP_STATE_PEER_REACHABLE:
        {
            uint32_t ind_buff = 0;
            err_code = get_packet_buffer_index(&ind_buff, (uint32_t *)&m_invalid_pkt_seq_num);
            if (err_code == NRF_ERROR_NOT_FOUND)
            {
                // Buffer of expected packets full, checking if peer is reachable.
                APPL_LOG("[APPL]: %ld packets transmitted, %d packets lost. Resetting counter. \r\n", \
                             m_pkt_seq_num, PACKET_BUFFER_LEN);
                m_node_state    = APP_STATE_IPV6_IF_UP;
                m_display_state = LEDS_TX_ECHO_REQUEST;
                m_pkt_seq_num = 0;
                memset(&m_packet_buffer[0][0], 0x00, sizeof(m_packet_buffer));
                err_code = app_timer_start(m_tx_node_timer, APP_PING_INTERVAL, NULL);
                APP_ERROR_CHECK(err_code);
                return;
            }

            ++m_pkt_seq_num;
            if (m_pkt_seq_num == INVALID_PACKET_SEQ_NUMBER)
            {
                ++m_pkt_seq_num;
            }

            test_packet_payload_t packet;
            uint8_t encoded_seq_num[TEST_PACKET_NUM_LEN];
            UNUSED_VARIABLE(uint32_encode(m_pkt_seq_num, &encoded_seq_num[0]));
            // The first 4 bytes of the payload is the packet sequence number.
            memcpy(&packet.packet_seq_num[0], &encoded_seq_num[0], TEST_PACKET_NUM_LEN);
            // The rest of the payload is random bytes.
            do
            {
                err_code = sd_rand_application_vector_get(&packet.packet_data[0], \
                                                          sizeof(packet.packet_data));
            }
            while (err_code == NRF_ERROR_SOC_RAND_NOT_ENOUGH_VALUES);
            APP_ERROR_CHECK(err_code);

            iot_pbuffer_alloc_param_t   pbuff_param;
            iot_pbuffer_t             * p_buffer;

            pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
            pbuff_param.type   = UDP6_PACKET_TYPE;
            pbuff_param.length = TEST_PACKET_PAYLOAD_LEN;

            // Allocate packet buffer.
            err_code = iot_pbuffer_allocate(&pbuff_param, &p_buffer);
            APP_ERROR_CHECK(err_code);

            memcpy(p_buffer->p_payload, &packet.packet_seq_num[0], TEST_PACKET_NUM_LEN);
            memcpy(p_buffer->p_payload+TEST_PACKET_NUM_LEN, &packet.packet_data[0], TEST_PACKET_DATA_LEN);

            ipv6_addr_t dest_ipv6_addr;
            memset(&dest_ipv6_addr, 0x00, sizeof(ipv6_addr_t));
            memcpy(&dest_ipv6_addr.u8[0], (uint8_t[]){SERVER_IPV6_ADDRESS}, IPV6_ADDR_SIZE);

            // Transmit UDP6 packet.
            err_code = udp6_socket_sendto(&m_udp_socket, &dest_ipv6_addr, HTONS(UDP_PORT), p_buffer);
            APP_ERROR_CHECK(err_code);

            APPL_LOG("[APPL]: Transmitted UDP packet sequence number: %ld\r\n", m_pkt_seq_num);

            // Store sent packet amongst expected packets.
            memcpy(&m_packet_buffer[ind_buff][0], &packet.packet_seq_num[0], TEST_PACKET_NUM_LEN);
            memcpy(&m_packet_buffer[ind_buff][TEST_PACKET_NUM_LEN], &packet.packet_data[0], TEST_PACKET_DATA_LEN);

            if (m_pkt_seq_num == 1)
            {
                err_code = app_timer_start(m_tx_node_timer, (TX_INTERVAL*5), NULL); // Slow start.
                APP_ERROR_CHECK(err_code);
            }
            else
            {
                err_code = app_timer_start(m_tx_node_timer, TX_INTERVAL, NULL);
                APP_ERROR_CHECK(err_code);
            }

            break;
        }
        default:
        {
            break;
        }
    }
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
            LEDS_OFF(LED_TWO);
            break;
        }
        case LEDS_TX_ECHO_REQUEST:
        {
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            break;
        }
        case LEDS_TX_UDP_PACKET:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_ON(LED_TWO);
            break;
        }
        default:
        {
            break;
        }
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

    // Create timer to send Echo Requests and UDP6 packets to peer.
    err_code = app_timer_create(&m_tx_node_timer, APP_TIMER_MODE_SINGLE_SHOT, tx_timeout_handler);
    APP_ERROR_CHECK(err_code);
    // Create timer to control board LEDs.
    err_code = app_timer_create(&m_led_blink_timer, APP_TIMER_MODE_REPEATED, blink_timeout_handler);
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

    APP_ERROR_CHECK(process_result);

    // Print PORTs
    // APPL_LOG("[APPL]: UDP Destination port: %lx\r\n", HTONS(p_udp_header->destport));
    // APPL_LOG("[APPL]: UDP Source port: %lx\r\n",      HTONS(p_udp_header->srcport));

    uint32_t rx_sequence_num = uint32_decode(&p_rx_packet->p_payload[0]);
    uint32_t ind_buff = 0;
    uint32_t err_code = get_packet_buffer_index(&ind_buff, &rx_sequence_num);
    if (err_code == NRF_ERROR_NOT_FOUND)
    {
        // Received packet sequence number is not found amongst expected packets.
        return NRF_SUCCESS;
    }

    if (0 == memcmp(p_rx_packet->p_payload, &m_packet_buffer[ind_buff][0], TEST_PACKET_PAYLOAD_LEN))
    {
        // If received packet is as expected, free slot in buffer.
        memset(&m_packet_buffer[ind_buff][0], 0x00, TEST_PACKET_PAYLOAD_LEN);
    }

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
            break;
        case ICMP6_TYPE_ECHO_REPLY:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");

            ipv6_addr_t server_ipv6_addr;
            memset(&server_ipv6_addr, 0, sizeof(ipv6_addr_t));
            memcpy(&server_ipv6_addr.u8[0], (uint8_t []){SERVER_IPV6_ADDRESS}, IPV6_ADDR_SIZE);

            if (0 == memcmp(&p_ip_header->srcaddr.u8[0], &server_ipv6_addr.u8[0], IPV6_ADDR_SIZE))
            {
                // Echo Response received from peer, start sending UDP6 packets.
                m_node_state    = APP_STATE_PEER_REACHABLE;
                m_display_state = LEDS_TX_UDP_PACKET;

                err_code = app_timer_start(m_tx_node_timer, TX_INTERVAL, NULL);
                APP_ERROR_CHECK(err_code);
            }

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

            memset(&m_packet_buffer[0][0], 0x00, sizeof(m_packet_buffer));
            m_node_state    = APP_STATE_IPV6_IF_UP;
            m_display_state = LEDS_TX_ECHO_REQUEST;

            // IPv6 interface is up, start sending Echo Requests to peer.
            err_code = app_timer_start(m_tx_node_timer, APP_PING_INTERVAL, NULL);
            APP_ERROR_CHECK(err_code);

            break;
        case IPV6_EVT_INTERFACE_DELETE:
            err_code = app_timer_stop(m_tx_node_timer);
            APP_ERROR_CHECK(err_code);

            APPL_LOG("[APPL]: Interface removed!\r\n");

            err_code = udp6_socket_free(&m_udp_socket);
            APP_ERROR_CHECK(err_code);

            memset(&m_packet_buffer[0][0], 0x00, sizeof(m_packet_buffer));
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
    err_code = app_timer_start(m_led_blink_timer, LED_BLINK_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            APPL_LOG ("[APPL]: Connected.\r\n");
            m_display_state = LEDS_IPV6_IF_DOWN;

            break;
        case BLE_GAP_EVT_DISCONNECTED:
            err_code = app_timer_stop(m_tx_node_timer);
            APP_ERROR_CHECK(err_code);

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
