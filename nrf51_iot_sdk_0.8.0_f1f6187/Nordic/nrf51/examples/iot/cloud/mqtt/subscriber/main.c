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
 * @defgroup iot_sdk_app_xively_mqtt_client main.c
 * @{
 * @ingroup iot_sdk_app_lwip
 *
 * @brief This file contains the source code for LwIP based MQTT Client sample application
 *        that uses Xively cloud service. Example subscribes to specific topic, and is notified
 *        about any changes in this topic.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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
#include "mqtt.h"
#include "lwip/timers.h"
#include "iot_timer.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"

#define DEVICE_NAME                         "MQTTXivelySubscriber"                                  /**< Device name used in BLE undirected advertisement. */

#define APP_TIMER_PRESCALER                 NRF51_DRIVER_TIMER_PRESCALER                            /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS                3                                                       /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE             4

#define APP_XIVELY_CONNECT_DELAY            APP_TIMER_TICKS(500, APP_TIMER_PRESCALER)               /**< MQTT Connection delay. */

#define LWIP_SYS_TICK_MS                    100                                                     /**< Interval for timer used as trigger to send. */
#define LED_BLINK_INTERVAL_MS               300                                                     /**< LED blinking interval. */

#define SCHED_MAX_EVENT_DATA_SIZE           128                                                     /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE                    12                                                      /**< Maximum number of events in the scheduler queue. */

#define LED_ONE                             BSP_LED_0_MASK
#define LED_TWO                             BSP_LED_1_MASK
#define LED_THREE                           BSP_LED_2_MASK
#define LED_FOUR                            BSP_LED_3_MASK
#define ALL_APP_LED                        (BSP_LED_0_MASK | BSP_LED_1_MASK | \
                                            BSP_LED_2_MASK | BSP_LED_3_MASK)                        /**< Define used for simultaneous operation of all application LEDs. */

#define APP_ADV_TIMEOUT                     0                                                       /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL                MSEC_TO_UNITS(100, UNIT_0_625_MS)                       /**< The advertising interval. This value can vary between 100ms to 10.24s). */

#define DEAD_BEEF                           0xDEADBEEF                                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define APPL_LOG                            app_trace_log                                           /**< Macro for logging application messages on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */
#define APPL_DUMP                           app_trace_dump                                          /**< Macro for dumping application data on UART, in case ENABLE_DEBUG_LOG_SUPPORT is not defined, no logging occurs. */

#define APP_DATA_MAX_SIZE                   TCP_MSS                                                 /**< Maximum size of data buffer size. */

#define APP_MQTT_XIVELY_BROKER_PORT         1883                                                    /**< Port number of MQTT Broker. */
#define APP_MQTT_XIVELY_CHANNEL             "Thermometer"                                           /**< Device's Channel. */
#define APP_MQTT_XIVELY_FEED_ID             "{PUT FEED ID HERE}"                                    /**< Device's Feed ID. */
#define APP_MQTT_XIVELY_API_KEY             "{PUT API KEY HERE}"                                    /**< API key used for authentication. */

// URL of given Xively JSON resource.
#define APP_MQTT_XIVELY_API_URL             "/v2/feeds/" APP_MQTT_XIVELY_FEED_ID ".json"

typedef enum
{
    LEDS_INACTIVE = 0,
    LEDS_BLE_ADVERTISING,
    LEDS_IPV6_IF_DOWN,
    LEDS_IPV6_IF_UP,
    LEDS_CONNECTED_TO_BROKER,
    LEDS_SUBSCRIBED
} display_state_t;

eui64_t                                     eui64_local_iid;                                        /**< Local EUI64 value that is used as the IID for*/
static ble_gap_adv_params_t                 m_adv_params;                                           /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t                       m_mqtt_conn_timer_id;                                   /**< Timer for delaying MQTT Connection. */
static app_timer_id_t                       m_iot_timer_tick_src_id;                                /**< App timer instance used to update the IoT timer wall clock. */

static display_state_t                      m_display_state    = LEDS_INACTIVE;                     /**< Board LED display state. */
static bool                                 m_interface_state  = false;                             /**< Actual status of interface. */
static bool                                 m_connection_state = false;                             /**< MQTT Connection state. */

static mqtt_client_t                        m_app_mqtt_id;                                          /**< MQTT Client instance reference provided by the MQTT module. */
static uint32_t                             m_packet_id        = 0;                                 /**< MQTT packet number. */
static const char                           m_device_id[]      = "nrfSubscriber-51";                /**< Unique MQTT client identifier. */
static const char                           m_user[]           = APP_MQTT_XIVELY_API_KEY;           /**< MQTT user name. */
static char                                 m_data_rcv[APP_DATA_MAX_SIZE];                          /**< Buffer used for parsing data. */

// Address of Xively cloud.
static const ip6_addr_t                     m_broker_addr =
{
    .addr =
    {
       HTONL(0x20010778),
       HTONL(0x0000ffff),
       HTONL(0x00640000),
       HTONL(0xd834e97a)
    }
};

static void app_xively_subscribe(void);

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


/**@brief Timer callback used for controlling board LEDs to represent application state.
 *
 */
static void blink_timeout_handler(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);

    switch (m_display_state)
    {
        case LEDS_INACTIVE:
        {
            LEDS_OFF(ALL_APP_LED);
            break;
        }
        case LEDS_BLE_ADVERTISING:
        {
            LEDS_INVERT(LED_ONE);
            LEDS_OFF((LED_TWO | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_IPV6_IF_DOWN:
        {
            LEDS_INVERT(LED_TWO);
            LEDS_OFF((LED_ONE | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_IPV6_IF_UP:
        {
            LEDS_ON(LED_ONE);
            LEDS_OFF((LED_TWO | LED_THREE | LED_FOUR));
            break;
        }
        case LEDS_CONNECTED_TO_BROKER:
        {
            LEDS_ON(LED_TWO);
            LEDS_OFF((LED_ONE | LED_THREE | LED_FOUR));
            m_display_state = LEDS_SUBSCRIBED;
            break;
        }
        case LEDS_SUBSCRIBED:
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

    // Check if interface has been established.
    if(m_interface_state == false)
    {
        return;
    }

    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case BSP_BUTTON_0:
            {
                if (m_connection_state == false)
                {
                    err_code = app_timer_start(m_mqtt_conn_timer_id, APP_XIVELY_CONNECT_DELAY, NULL);
                    APP_ERROR_CHECK(err_code);
                }
                break;
            }
            case BSP_BUTTON_1:
            {
                if (m_connection_state == true)
                {
                    err_code = mqtt_disconnect(&m_app_mqtt_id);
                    APP_ERROR_CHECK(err_code);
                }
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
static void button_init(void)
{
    uint32_t err_code;

    static app_button_cfg_t buttons[] =
    {
        {BSP_BUTTON_0, false, BUTTON_PULL, button_event_handler},
        {BSP_BUTTON_1, false, BUTTON_PULL, button_event_handler},
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


/**@brief Asynchronous event notification callback registered by the application with the module to receive MQTT events. */
static void app_mqtt_evt_handler(const mqtt_client_t * p_client, const mqtt_evt_t * p_evt)
{
    uint32_t err_code;
    switch(p_evt->id)
    {
        case MQTT_EVT_CONNECTED:
        {
            APPL_LOG ("[APPL]: >> MQTT_EVT_CONNECTED\r\n");

            if(p_evt->result == MQTT_SUCCESS)
            {
                m_connection_state = true;
                m_display_state = LEDS_CONNECTED_TO_BROKER;

                // Subscribe to MQTT Broker.
                app_xively_subscribe();
            }
            break;
        }
        case MQTT_EVT_DATA_RX:
        {
            APPL_LOG ("[APPL]: >> MQTT_EVT_DATA_RX\r\n");

            char     * p_fragment;
            char     * p_data_start;
            uint16_t   temperature;

            memcpy(m_data_rcv, p_evt->param.rx_param.p_data->p_data, p_evt->param.rx_param.p_data->data_len);
            m_data_rcv[p_evt->param.rx_param.p_data->data_len] = 0;

            // Find fragment containing current value of Temperature.
            p_fragment   = strstr(m_data_rcv, "\"current_value\"");
            p_data_start = strtok(p_fragment, ":");
            p_data_start = strtok(NULL, ",");

            p_data_start    = p_data_start + 1;
            p_data_start[5] = 0;

            // Change string to uint16_t value.
            temperature = 10 * (p_data_start[0] - '0') + (p_data_start[1] - '0');

            // Change leds sate to indicate actual data that has been sent.
            switch(temperature % 4)
            {
              case 0:
                LEDS_OFF(LED_THREE | LED_FOUR);
                break;
              case 1:
                LEDS_ON(LED_FOUR);
                LEDS_OFF(LED_THREE);
                break;
              case 2:
                LEDS_ON(LED_THREE);
                LEDS_OFF(LED_FOUR);
                break;
              case 3:
                LEDS_ON(LED_THREE | LED_FOUR);
                break;
            }

            APPL_LOG ("[APPL]: Actual value of channel Temperature: %s\r\n", p_data_start);

          break;
        }
        case MQTT_EVT_DISCONNECTED:
        {
            APPL_LOG ("[APPL]: >> MQTT_EVT_DISCONNECTED\r\n");
            m_connection_state = false;
            m_display_state    = LEDS_IPV6_IF_UP;

            // Make reconnection in case of tcp problem.
            if(p_evt->result == MQTT_ERR_TRANSPORT_CLOSED)
            {
                err_code = app_timer_start(m_mqtt_conn_timer_id, APP_XIVELY_CONNECT_DELAY, NULL);
                APP_ERROR_CHECK(err_code);
            }

            break;
        }
        default:
            break;
    }
}


/**@brief Function for subscribing to specific MQTT topic.
 *
 * @details Subscribe to MQTT Xively topic.
 */
static void app_xively_subscribe(void)
{
    uint32_t err_code;

    if (m_connection_state == false)
    {
        return;
    }

    // Create MQTT topic.
    mqtt_topic_t topic;
    topic.p_topic   = (uint8_t *)APP_MQTT_XIVELY_API_URL;
    topic.topic_len = strlen(APP_MQTT_XIVELY_API_URL);

    err_code = mqtt_subscribe(&m_app_mqtt_id, &topic, m_packet_id++);

    // Check if there is no pending transaction.
    if(err_code != MQTT_ERR_BUSY)
    {
      APP_ERROR_CHECK(err_code);
    }

    APPL_LOG("[APPL]: mqtt_subscribe result 0x%08lx\r\n", err_code);
}


/**@brief Timer callback used for connecting to Xively cloud.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void app_xively_connect(void * p_context)
{
    UNUSED_VARIABLE(p_context);

    uint32_t       err_code;
    mqtt_connect_t param;

    param.broker_addr    = m_broker_addr;
    param.broker_port    = APP_MQTT_XIVELY_BROKER_PORT;
    param.evt_cb         = app_mqtt_evt_handler;
    param.device_id      = m_device_id;
    param.p_password     = NULL;
    param.p_user_name    = m_user;

    err_code = mqtt_connect(&m_app_mqtt_id, &param);
    APP_ERROR_CHECK(err_code);
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

    mqtt_init();
}


/**@brief Timer callback used for periodic servicing of LwIP protocol timers.
 *        This trigger is also used in the example to trigger sending TCP Connection.
 *
 * @details Timer callback used for periodic servicing of LwIP protocol timers.
 *
 */
static void app_lwip_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    UNUSED_PARAMETER(wall_clock_value);
    sys_check_timeouts();
    UNUSED_VARIABLE(mqtt_live());
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
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    // Create timer to create delay connection to Xively.
    err_code = app_timer_create(&m_mqtt_conn_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                app_xively_connect);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {app_lwip_time_tick,          LWIP_SYS_TICK_MS},
        {blink_timeout_handler,       LED_BLINK_INTERVAL_MS}
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

    // Create app timer to serve as tick source.
    err_code = app_timer_create(&m_iot_timer_tick_src_id,
                                APP_TIMER_MODE_REPEATED,
                                iot_timer_tick_callback);
    APP_ERROR_CHECK(err_code);
    
    // Starting the app timer instance that is the tick source for the IoT Timer.
    err_code = app_timer_start(m_iot_timer_tick_src_id, \
                               APP_TIMER_TICKS(IOT_TIMER_RESOLUTION_IN_MS, APP_TIMER_PRESCALER), \
                               NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf51_driver_interface_up(void)
{
    APPL_LOG ("[APPL]: IPv6 Interface Up.\r\n");

    sys_check_timeouts();

    // Set flag indicating interface state.
    m_interface_state = true;
    m_display_state = LEDS_IPV6_IF_UP;
}


/**@brief Function to handle interface down event. */
void nrf51_driver_interface_down(void)
{
    APPL_LOG ("[APPL]: IPv6 Interface Down.\r\n");

    // Clear flag indicating interface state.
    m_interface_state = false;
    m_display_state = LEDS_IPV6_IF_DOWN;

    // A system reset is issued, becuase a defect 
    // in lwIP prevents recovery -- see SDK release notes for details.
    NVIC_SystemReset();
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
    scheduler_init();
    timers_init();
    iot_timer_init();
    button_init();
    ble_stack_init();
    advertising_init();
    ip_stack_init ();

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
