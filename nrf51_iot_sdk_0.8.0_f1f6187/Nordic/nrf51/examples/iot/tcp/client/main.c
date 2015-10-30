
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
 * @defgroup iot_sdk_app_tcp_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 *
 * @brief This file contains the source code for LwIP TCP Client sample application.
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "app_timer_appsh.h"
#include "app_scheduler.h"
#include "app_button.h"
#include "nordic_common.h"
#include "softdevice_handler_appsh.h"
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
/*lint -save -e607 */
#include "lwip/tcp.h"
/*lint -restore */
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"

#define DEVICE_NAME                         "LwIPTCPClient"                                         /**< Device name used in BLE undirected advertisement. */

/** Remote TCP Port Address on which data is transmitted.
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
#define APP_TIMER_MAX_TIMERS                3                                                       /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE             3
#define LWIP_SYS_TIMER_INTERVAL             APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)               /**< Interval for timer used as trigger to send. */
#define APP_TCP_DATA_TIMER_INTERVAL         APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)                /**< Interval for timer used as trigger to send. */

#define SCHED_MAX_EVENT_DATA_SIZE           128                                                     /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                    12                                                      /**< Maximum number of events in the scheduler queue. */

#define ADVERTISING_LED                     BSP_LED_0_MASK                                          /**< Is on when device is advertising. */
#define CONNECTED_LED                       BSP_LED_1_MASK                                          /**< Is on when device is connected. */
#define TCP_CONNECTED_LED                   BSP_LED_2_MASK                                          /**< Is on when device is connected. */
#define DISPLAY_LED_0                       BSP_LED_0_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_1                       BSP_LED_1_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_2                       BSP_LED_2_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define DISPLAY_LED_3                       BSP_LED_3_MASK                                          /**< LED used for displaying mod 4 of data payload received on TCP port. */
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

#define APP_ADV_TIMEOUT                     0                                                       /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL                MSEC_TO_UNITS(100, UNIT_0_625_MS)                       /**< The advertising interval. This value can vary between 100ms to 10.24s). */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define TCP_SERVER_PORT                     9000                                                    /**< Remote server port to which this application requests a connection. */
#define TCP_CLIENT_PORT                     9001                                                    /**< Local client port number used by the applciation. */
#define TCP_DATA_SIZE                       8                                                       /**< TCP Data size sent on remote. */

typedef enum
{
    TCP_STATE_IDLE,
    TCP_STATE_REQUEST_CONNECTION,
    TCP_STATE_CONNECTED,
    TCP_STATE_DATA_TX_IN_PROGRESS,
    TCP_STATE_TCP_SEND_PENDING,
    TCP_STATE_DISCONNECTED
}tcp_state_t;

eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ble_gap_adv_params_t                 m_adv_params;                                           /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t                       m_sys_timer_id;                                         /**< System Timer used to service LwIP timers periodically. */
static app_timer_id_t                       m_tcp_data_timer_id;                                    /**< TCP Data Timer used to send ping request periodically. */
static struct tcp_pcb                     * mp_tcp_port;                                            /**< TCP Port to listen on. */
static tcp_state_t                          m_tcp_state;                                            /**< TCP State information. */

static uint32_t                             m_sequence_number = 0;                                  /**< Counter used for sequencing data packets transmitted. */

static void tcp_request_connection(void);
static void tcp_send_data(struct tcp_pcb * p_pcb);

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
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n", error_code, line_num, p_file_name);

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


static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{
    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case BSP_BUTTON_0:
            {
                tcp_request_connection();
                break;
            }
            case BSP_BUTTON_1:
            {
                if (m_tcp_state == TCP_STATE_CONNECTED)
                {
                    uint32_t err_code = app_timer_start(m_tcp_data_timer_id, APP_TCP_DATA_TIMER_INTERVAL, NULL);
                    APP_ERROR_CHECK(err_code);

                    m_tcp_state = TCP_STATE_DATA_TX_IN_PROGRESS;
                }
                else if ((m_tcp_state == TCP_STATE_DATA_TX_IN_PROGRESS) || 
                         (m_tcp_state == TCP_STATE_TCP_SEND_PENDING))
                {
                    uint32_t err_code = app_timer_stop(m_tcp_data_timer_id);
                    APP_ERROR_CHECK(err_code);

                    m_tcp_state = TCP_STATE_CONNECTED;
                }
                break;
            }
            default:
                break;
        }
    }
}


static void button_init(void)
{
    uint32_t err_code;

    // Configure HR_INC_BUTTON_PIN_NO and HR_DEC_BUTTON_PIN_NO as wake up buttons and also configure
    // for 'pull up' because the eval board does not have external pull up resistors connected to
    // the buttons.
    static app_button_cfg_t buttons[] =
    {
        {BSP_BUTTON_0, false, BUTTON_PULL, button_event_handler},
        {BSP_BUTTON_1, false, BUTTON_PULL, button_event_handler}
    };

    #define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
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
    SOFTDEVICE_HANDLER_APPSH_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, true);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief TCP Port Write complete callback.
 *
 * @details Calbback registered to be notified of the write complete event on the TCP port.
 *          In case write complete is notified with 'zero' length, port is closed.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 * @param[in]   len       Length of data written successfully.
 *
 * @retval ERR_OK.
 */
static err_t tcp_write_complete(void           * p_arg,
                                struct tcp_pcb * p_pcb,
                                u16_t            len)
{
    UNUSED_PARAMETER(p_arg);
    UNUSED_PARAMETER(p_pcb);
    
    if (m_tcp_state == TCP_STATE_TCP_SEND_PENDING)
    {
        m_tcp_state = TCP_STATE_DATA_TX_IN_PROGRESS;
    }

    return ERR_OK;
}


/**@brief Send test data on the port.
 *
  * @details Sends TCP data in Request of size 8 in format described in description above.
 *
 * @param[in]   p_pcb     PCB identifier of the port.
 */
static void tcp_send_data(struct tcp_pcb * p_pcb)
{
    err_t  err = ERR_OK;

    APPL_LOG ("[APPL]: >> TCP Tx Data.\r\n");
    
    if (m_tcp_state == TCP_STATE_TCP_SEND_PENDING)
    {
        return;
    }

    uint32_t len = tcp_sndbuf(p_pcb);

    APPL_LOG ("[APPL]: Available TCP length 0x%08lx\r\n", len);

    if (len >= TCP_DATA_SIZE) 
    {
        m_sequence_number++;

        //Register callback to get notification of data reception is complete.
        tcp_sent(p_pcb, tcp_write_complete);
        uint8_t tcp_data[TCP_DATA_SIZE];

        tcp_data[0] = (uint8_t )((m_sequence_number >> 24) & 0x000000FF);
        tcp_data[1] = (uint8_t )((m_sequence_number >> 16) & 0x000000FF);
        tcp_data[2] = (uint8_t )((m_sequence_number >> 8)  & 0x000000FF);
        tcp_data[3] = (uint8_t )(m_sequence_number         & 0x000000FF);

        tcp_data[4] = 'P';
        tcp_data[5] = 'i';
        tcp_data[6] = 'n';
        tcp_data[7] = 'g';

        //Enqueue data for transmission.
        err = tcp_write(p_pcb, tcp_data, TCP_DATA_SIZE, 1);

        if (err != ERR_OK)
        {
            APPL_LOG ("[APPL]: Failed to send TCP packet, reason %d\r\n", err);
            m_sequence_number--;
        }
        else
        {
            m_tcp_state = TCP_STATE_TCP_SEND_PENDING;
            APPL_LOG ("[APPL]: Data Tx, Sequence number 0x%08lx\r\n", m_sequence_number);
        }
    }
    else
    {
        APPL_LOG ("[APPL]: No room to send next request, available length 0x%08lx\r\n", len);
    }


    APPL_LOG ("[APPL]: << TCP Tx Data.\r\n");
}


/**@brief Callback registered for receiving data on the TCP Port.
 *
 * @param[in]   p_arg     Receive argument set on the TCP port.
 * @param[in]   p_pcb     TCP PCB on which data is received.
 * @param[in]   p_buffer  Buffer with received data.
 * @param[in]   err       Event result indicating error associated with the receive,
 *                        if any, else ERR_OK.
 */
err_t tcp_recv_data_handler(void           * p_arg,
                            struct tcp_pcb * p_pcb,
                            struct pbuf    * p_buffer,
                            err_t            err)
{
    APPL_LOG ("[APPL]: >> TCP Data.\r\n");

    //Check event result before proceeding.
    if (err == ERR_OK)
    {
        uint8_t *p_data = p_buffer->payload;

        if (p_buffer->len == TCP_DATA_SIZE)
        {
            uint32_t rx_seq_number = 0;

            rx_seq_number  = ((p_data[0] << 24) & 0xFF000000);
            rx_seq_number |= ((p_data[1] << 16) & 0x00FF0000);
            rx_seq_number |= ((p_data[2] << 8)  & 0x0000FF00);
            rx_seq_number |= (p_data[3]         & 0x000000FF);

            LEDS_OFF(ALL_APP_LED);

            if (rx_seq_number & 0x00000001)
            {
                LEDS_ON(DISPLAY_LED_0);
            }
            if (rx_seq_number & 0x00000002)
            {
                LEDS_ON(DISPLAY_LED_1);
            }
            if (rx_seq_number & 0x00000004)
            {
                LEDS_ON(DISPLAY_LED_2);
            }
            if (rx_seq_number & 0x00000008)
            {
                LEDS_ON(DISPLAY_LED_3);
            }

            if (rx_seq_number != m_sequence_number)
            {
                APPL_LOG ("[APPL]: Mismatch in sequence number.\r\n");
            }
        }
        // All is good with the data received, process it.
        tcp_recved(p_pcb, p_buffer->tot_len);
        UNUSED_VARIABLE(pbuf_free(p_buffer));
    }
    else
    {
        //Free the buffer in case its not NULL.
        if (p_buffer != NULL)
        {
            UNUSED_VARIABLE(pbuf_free(p_buffer));
        }
    }

    return ERR_OK;
}


/**@brief Callback registered To be notified of errors on the TCP ports.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   err       Indication of nature of error that occurred on the port.
 */
static void tcp_error_handler(void *p_arg, err_t err)
{
    LWIP_UNUSED_ARG(p_arg);
    LWIP_UNUSED_ARG(err);

    APPL_LOG ("[APPL]: Error on TCP port, reason 0x%08x\r\n", err);
}


/**@brief TCP Poll Callback.
 *
 * @details Callback registered for TCP port polling, currently does nothing.
 *
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 *
 * @retval ERR_OK.
 */
static err_t tcp_connection_poll(void *p_arg, struct tcp_pcb *p_pcb)
{
    LWIP_UNUSED_ARG(p_arg);
    LWIP_UNUSED_ARG(p_pcb);

    return ERR_OK;
}


/**@brief TCP Port Connection Callback.
 *
 * @details Callback registered with TCP for connection complete. err indicates if the
 * @param[in]   p_arg     Receive argument set on the port.
 * @param[in]   p_pcb     PCB identifier of the port.
 * @param[in]   err       Event result indicating error associated with the receive,
 *                        if any, else ERR_OK.
 */
static err_t tcp_connection_callback(void           * p_arg,
                                     struct tcp_pcb * p_pcb,
                                     err_t            err)
{
    APPL_LOG ("[APPL]: >> TCP Connected, result 0x%08X.\r\n", err);

    //Ensure connection establishment was successful.
    APP_ERROR_CHECK(err);

    //Set the state of TCP port and associated handlers/information.
    m_tcp_state = TCP_STATE_CONNECTED;

    tcp_setprio(p_pcb, TCP_PRIO_MIN);
    tcp_arg(p_pcb, NULL);
    tcp_recv(p_pcb, tcp_recv_data_handler);
    tcp_err(p_pcb, tcp_error_handler);
    tcp_poll(p_pcb, tcp_connection_poll, 0);

    LEDS_ON(TCP_CONNECTED_LED);

    return ERR_OK;
}


/**@brief Setup TCP Port.
 *
 * @details Set up TCP Port and register receive callback.
 */
static void tcp_port_setup(void)
{
    ip6_addr_t any_addr;
    ip6_addr_set_any(&any_addr);

    mp_tcp_port = tcp_new_ip6();

    if (mp_tcp_port != NULL)
    {
        err_t err = tcp_bind_ip6(mp_tcp_port, &any_addr, TCP_CLIENT_PORT);
        APP_ERROR_CHECK(err);
    }
    else
    {
        ASSERT(0);
    }
}


/**@brief Request TCP Port Connection.
 *
 * @details Request TCP Port Connection, connection procedure is complete when
 *          tcp_connected_callback called.
 */
void tcp_request_connection(void)
{
    err_t err = tcp_connect_ip6(mp_tcp_port, &m_remote_addr, TCP_SERVER_PORT, tcp_connection_callback);
    APP_ERROR_CHECK(err);
    APPL_LOG("[APPL]: >> TCP Connection Requested to remote addr.\r\n");
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


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void system_timer_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    sys_check_timeouts();
}

/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void tcp_request_timer_callback(void * p_context)
{
    UNUSED_VARIABLE(p_context);
    tcp_send_data(mp_tcp_port);
}



/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    // Create timers.
    err_code = app_timer_create(&m_sys_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                system_timer_callback);

    // Create timers.
    err_code = app_timer_create(&m_tcp_data_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                tcp_request_timer_callback);

    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf51_driver_interface_up(void)
{
    uint32_t err_code;

    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    m_tcp_state = TCP_STATE_REQUEST_CONNECTION;

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

    LEDS_OFF((DISPLAY_LED_0 | DISPLAY_LED_1 | DISPLAY_LED_2 | DISPLAY_LED_3));
    LEDS_ON(ADVERTISING_LED);

    m_tcp_state = TCP_STATE_DISCONNECTED;
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
    button_init();
    ble_stack_init();
    advertising_init();
    ip_stack_init ();
    scheduler_init();
    tcp_port_setup();

    APPL_LOG ("\r\n");
    APPL_LOG ("[APPL]: Init done.\r\n");

    //Start execution.
    advertising_start();

    //Enter main loop.
    for (;;)
    {
        //Execute event schedule.
        app_sched_execute();

        //Sleep waiting for an application event.
        err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);
    }
}

/**
 * @}
 */
