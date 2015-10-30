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
 * @defgroup iot_sdk_dns6_app main.c
 * @{
 * @ingroup iot_sdk_app_ipv6
 * @brief This file contains the source code for Nordic's IPv6 DNS6 sample application.
 *
 */


#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "nrf_delay.h"
#include "softdevice_handler.h"
#include "mem_manager.h"
#include "app_trace.h"
#include "app_timer.h"
#include "app_button.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_ipsp.h"
#include "iot_common.h"
#include "iot_timer.h"
#include "ipv6_api.h"
#include "icmp6_api.h"
#include "dns6_api.h"

#define DEVICE_NAME                     "IPv6DNS"                                                   /**< Device name used in BLE undirected advertisement. */

#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK

#define START_BUTTON_PIN_NO             BSP_BUTTON_0                                                /**< Button used to start application state machine. */
#define STOP_BUTTON_PIN_NO              BSP_BUTTON_1                                                /**< Button used to stop application state machine. */

#define APP_TIMER_PRESCALER             31                                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            3                                                           /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         5                                                           /**< Size of timer operation queues. */

#define APP_ADV_TIMEOUT                 0                                                           /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL            MSEC_TO_UNITS(333, UNIT_0_625_MS)                           /**< The advertising interval. This value can vary between 100ms to 10.24s). */

#define LED_BLINK_INTERVAL_MS           500                                                         /**< LED blinking interval. */
#define APP_STATE_INTERVAL              APP_TIMER_TICKS(1500, APP_TIMER_PRESCALER)                  /**< Interval for executing application state machine. */

#define APP_RTR_SOLICITATION_DELAY      1000                                                         /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME             128                                                         /**< Max length of filename to copy for the debug error handler. */

#define APP_HOSTNAME                    "example.com"                                               /**< Host name to perform DNS resolution and Echo Ping. */
#define APP_DNS_LOCAL_PORT              0x8888                                                      /**< UDP Port number of local DNS Resolver. */
#define APP_DNS_SERVER_PORT             0x0035                                                      /**< UDP Port number of DNS Server. */
#define APP_DNS_SERVER_ADDR             {{0x20, 0x01, 0x48, 0x60, 0x48, 0x60, 0x00, 0x00, \
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88}}          /**< IPv6 address of DNS Server. */

#define APP_MAX_ECHO_REQUEST_RTR        3                                                           /**< Maximum echo request retransmission before application goes back to querying state. */

#define APP_DISABLE_LOGS                0                                                           /**< Disable debug trace in the application. */

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

/**@brief Application's state. */
typedef enum
{
    APP_STATE_IDLE = 0,
    APP_STATE_QUERYING,
    APP_STATE_RESOLVING,
    APP_STATE_PINGING
} app_state_t;

/**@brief Led's indication state. */
typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_BLE_ADVERTISING,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP
} display_state_t;

static ble_gap_addr_t         m_local_ble_addr;                                                     /**< Local BLE address. */
static ble_gap_adv_params_t   m_adv_params;                                                         /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t         m_app_timer;                                                          /**< Timer instance used for application state machine. */
static app_timer_id_t         m_iot_timer_tick_src_id;                                              /**< App timer instance used to update the IoT timer wall clock. */
static iot_interface_t      * mp_interface = NULL;                                                  /**< Pointer to IoT interface if any. */
static ipv6_addr_t            m_hostname_address;                                                   /**< IPv6 address of given hostname. */
static uint32_t               m_echo_req_retry_count;                                               /**< Number of echo request retransmission. */
static display_state_t        m_display_state = LEDS_INACTIVE;                                      /**< Board LED display state. */
static volatile app_state_t   m_app_state = APP_STATE_IDLE;                                         /**< State of application state machine. */


/**@brief Addresses used in sample application. */
static const ipv6_addr_t      m_local_routers_multicast_addr = {{0xFF, 0x02, 0x00, 0x00, \
                                                                 0x00, 0x00, 0x00, 0x00, \
                                                                 0x00, 0x00, 0x00, 0x00, \
                                                                 0x00, 0x00, 0x00, 0x02}};          /**< Multicast address of all routers on the local network segment. */

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
    LEDS_ON((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n",
             error_code,
             line_num,
             p_file_name);

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
    // This function should NOT be used in a final product.
    // It is intended STRICTLY for development/debugging purposes.
    // The flash write will happen EVEN if the radio is active, thus interrupting
    // any communication.
    // Use with care. Un-comment the line below to use.
    // ble_debug_assert_handler(error_code, line_num, p_file_name);

    // On assert, the system can only recover on reset.
    // NVIC_SystemReset();

    for (;;)
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
    LEDS_CONFIGURE((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));

    // Turn leds off.
    LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
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
            break;
        }

        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_OFF((LED_THREE | LED_FOUR));
            LEDS_ON(LED_ONE);
            LEDS_INVERT(LED_TWO);
            break;
        }

        case LEDS_IPV6_IF_UP:
        {
            LEDS_OFF(LED_ONE);
            LEDS_ON(LED_TWO);
            break;
        }

        default:
        {
            break;
        }
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

    // Check if interface is UP.
    if(mp_interface == NULL)
    {
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case START_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Start button has been pushed.\r\n");

                // Change application state in case being in IDLE state.
                if(m_app_state == APP_STATE_IDLE)
                {
                    m_app_state = APP_STATE_QUERYING;

                    // Start application state machine timer.
                    err_code = app_timer_start(m_app_timer, APP_STATE_INTERVAL, NULL);
                    APP_ERROR_CHECK(err_code);
                }

                break;
            }
            case STOP_BUTTON_PIN_NO:
            {
                APPL_LOG("[APPL]: Stop button has been pushed.\r\n");
                LEDS_OFF((LED_THREE | LED_FOUR));

                // Back to IDLE state.
                m_app_state = APP_STATE_IDLE;

                // Stop application state machine timer.
                err_code = app_timer_stop(m_app_timer);
                APP_ERROR_CHECK(err_code);
                break;
            }

            default:
              break;
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
        {START_BUTTON_PIN_NO, false, BUTTON_PULL, button_event_handler},
        {STOP_BUTTON_PIN_NO,  false, BUTTON_PULL, button_event_handler}
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
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

    if (m_app_state != APP_STATE_RESOLVING)
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
                memcpy(m_hostname_address.u8, p_addr[0].u8, IPV6_ADDR_SIZE);

                // Change application state.
                m_app_state = APP_STATE_PINGING;

                // Turn on LED_THREE
                LEDS_ON(LED_THREE);
            }
        }
    }
    else
    {
        // Start application state machine from beginning.
        m_app_state = APP_STATE_QUERYING;
    }
}


/**@brief Application state machine used for sending DNS Query and Echo Request.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void app_fsm(void * p_context)
{
    uint32_t                   err_code;
    iot_pbuffer_t            * p_buffer;
    iot_pbuffer_alloc_param_t  pbuff_param;
    ipv6_addr_t                src_addr;

    switch(m_app_state)
    {
      case APP_STATE_QUERYING:
          APPL_LOG("[APPL]: Application in querying state.\r\n");
          // Set number of retransmission to 0.
          m_echo_req_retry_count = 0;

          // Turn off LED_THREE and LED_FOUR
          LEDS_OFF((LED_THREE | LED_FOUR));

          // Find all IPv6 address corresponds to APP_HOSTNAME domain.
          err_code = dns6_query(APP_HOSTNAME, app_dns_handler);
          APP_ERROR_CHECK(err_code);

          // Change state to resolving.
          m_app_state = APP_STATE_RESOLVING;
        break;
      case APP_STATE_RESOLVING:
          APPL_LOG("[APPL]: Application in resolving state.\r\n");
          // Wait for DNS response.
          break;
      case APP_STATE_PINGING:
          APPL_LOG("[APPL]: Application in pinging state.\r\n");

          if(m_echo_req_retry_count < APP_MAX_ECHO_REQUEST_RTR)
          {
              pbuff_param.flags  = PBUFFER_FLAG_DEFAULT;
              pbuff_param.type   = ICMP6_PACKET_TYPE;
              pbuff_param.length = ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET + 10;

              // Allocate packet buffer.
              err_code = iot_pbuffer_allocate(&pbuff_param, &p_buffer);
              APP_ERROR_CHECK(err_code);

              // Fill payload of Echo Request with 'A' letters.
              memset(p_buffer->p_payload + ICMP6_ECHO_REQUEST_PAYLOAD_OFFSET, 'A', 10);

              // Find proper source address for given destination one.
              err_code = ipv6_address_find_best_match(&mp_interface, &src_addr, &m_hostname_address);
              APP_ERROR_CHECK(err_code);

              // Send Echo Request to all nodes.
              err_code = icmp6_echo_request(mp_interface, &src_addr, &m_hostname_address, p_buffer);
              APP_ERROR_CHECK(err_code);

              // Increase retransmission number.
              m_echo_req_retry_count++;
          }
          else
          {
              APPL_LOG("[APPL]: Failed, no Echo Response received.\r\n");
              // Go to initial state.
              m_app_state = APP_STATE_QUERYING;
          }
          break;
      default:
        break;
    }
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
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module, making it use the scheduler
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timer to control application state machine.
    err_code = app_timer_create(&m_app_timer, APP_TIMER_MODE_REPEATED, \
                                app_fsm);
    APP_ERROR_CHECK(err_code);

    // Initialize timer instance as a tick source for IoT timer.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
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
    uint32_t                err_code;
    ble_advdata_t           advdata;
    uint8_t                 flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
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

    // Build and set advertising data.
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
    m_adv_params.p_peer_addr = NULL; // Undirected advertisement.
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
            APPL_LOG("[APPL]: Connected.\r\n");
            m_display_state = LEDS_IPV6_IF_DOWN;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            APPL_LOG("[APPL]: Disconnected.\r\n");
            advertising_start();
            break;

        default:
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
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


static void ip_app_handler(iot_interface_t * p_interface, ipv6_event_t * p_event)
{
    uint32_t    err_code;
    ipv6_addr_t src_addr;

    APPL_LOG("[APPL]: Got IP Application Handler Event on interface 0x%p\r\n", p_interface);

    switch (p_event->event_id)
    {
        case IPV6_EVT_INTERFACE_ADD:
            APPL_LOG("[APPL]: New interface added!\r\n");
            mp_interface = p_interface;

            m_display_state = LEDS_IPV6_IF_UP;

            APPL_LOG("[APPL]: Sending Router Solicitation to all routers!\r\n");

            // Create Link Local addresses
            IPV6_CREATE_LINK_LOCAL_FROM_EUI64(&src_addr, p_interface->local_addr.identifier);

            // Delay first solicitation due to possible restriction on other side.
            nrf_delay_ms(APP_RTR_SOLICITATION_DELAY);

            // Send Router Solicitation to all routers.
            err_code = icmp6_rs_send(p_interface,
                                     &src_addr,
                                     &m_local_routers_multicast_addr);
            APP_ERROR_CHECK(err_code);
        break;

        case IPV6_EVT_INTERFACE_DELETE:
            APPL_LOG("[APPL]: Interface removed!\r\n");
            mp_interface = NULL;

            m_display_state = LEDS_IPV6_IF_DOWN;

            // Stop application state machine timer.
            m_app_state = APP_STATE_IDLE;
            err_code = app_timer_stop(m_app_timer);
            APP_ERROR_CHECK(err_code);
            break;

        case IPV6_EVT_INTERFACE_RX_DATA:
            APPL_LOG("[APPL]: Got unsupported protocol data!\r\n");
            break;

        default:
            // Unknown event. Should not happen.
            break;
    }
}


/**@brief ICMP6 module event handler.
 *
 * @details Callback registered with the ICMP6 module to receive asynchronous events from
 *          the module, if the ICMP6_ENABLE_ALL_MESSAGES_TO_APPLICATION constant is not zero
 *          or the ICMP6_ENABLE_ND6_MESSAGES_TO_APPLICATION constant is not zero.
 */
uint32_t icmp6_handler(iot_interface_t * p_interface,
                       ipv6_header_t   * p_ip_header,
                       icmp6_header_t  * p_icmp_header,
                       uint32_t          process_result,
                       iot_pbuffer_t   * p_rx_packet)
{
    APPL_LOG("[APPL]: Got ICMP6 Application Handler Event on interface 0x%p\r\n", p_interface);

    APPL_LOG("[APPL]: Source IPv6 Address: ");
    APPL_ADDR(p_ip_header->srcaddr);
    APPL_LOG("[APPL]: Destination IPv6 Address: ");
    APPL_ADDR(p_ip_header->destaddr);
    APPL_LOG("[APPL]: Process result = 0x%08lx\r\n", process_result);

    switch (p_icmp_header->type)
    {
        case ICMP6_TYPE_ECHO_REPLY:
            APPL_LOG("[APPL]: ICMP6 Message Type = Echo Reply\r\n");

            if (m_app_state != APP_STATE_IDLE)
            {
                // Invert LED_FOUR.
                LEDS_INVERT(LED_FOUR);
            }

            // Reset echo request retransmission number.
            m_echo_req_retry_count = 0;
            break;
        default:
            break;
    }

    return NRF_SUCCESS;
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

    // Configure DNS client and server parameters.
    dns6_init_t dns_init_param =
    {
        .local_src_port  = 0x8888,
        .dns_server      =
        {
            .port = APP_DNS_SERVER_PORT,
            .addr = APP_DNS_SERVER_ADDR
        }
    };

    err_code = dns6_init(&dns_init_param);
}


/**@brief Function for initializing the transport of IPv6. */
static void ipv6_transport_init(void)
{
    ble_stack_init();
    advertising_init();
}


/**@brief Function for initializing the IoT Timer. */
static void ip_stack_timer_init(void)
{
    uint32_t err_code;

    const iot_timer_client_t list_of_clients[] =
    {
        {blink_timeout_handler, LED_BLINK_INTERVAL_MS},
        {dns6_timeout_process, SEC_TO_MILLISEC(DNS6_RETRANSMISSION_INTERVAL)}
    };

    // The list of IoT Timer clients is declared as a constant.
    const iot_timer_clients_list_t iot_timer_clients =
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


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    // Initialize
    app_trace_init();
    leds_init();

    timers_init();
    buttons_init();
    ipv6_transport_init();
    ip_stack_init();
    ip_stack_timer_init();

    APPL_LOG("\r\n");
    APPL_LOG("[APPL]: Init complete.\r\n");

    // Start execution
    advertising_start();

    // Enter main loop
    for (;;)
    {
        power_manage();
    }
}


/**
 * @}
 */
