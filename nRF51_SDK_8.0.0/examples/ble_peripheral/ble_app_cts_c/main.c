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
 * @defgroup ble_sdk_app_cts_c_main main.c
 * @{
 * @ingroup ble_sdk_app_cts_c
 * @brief Current Time Profile sample application.
 *
 * This file contains the source code for a sample application that uses Current Time Service.
 * This is the client role of the profile, implemented on a peripheral device.
 * When a central device connects, the application will trigger a security procedure (if this is not done
 * by the central side first). Completion of the security procedure will trigger a service
 * discovery. When the Current Time Service and Characteristic have been discovered on the
 * server, pressing button 1 will trigger a read of the current time and print it on the UART.
 *
 */

#include <stdint.h>
#include <string.h>
#include "app_error.h"
#include "app_scheduler.h"
#include "app_timer_appsh.h"
#include "app_trace.h"
#include "app_gpiote.h"
#include "ble.h"
#include "ble_cts_c.h"
#include "ble_db_discovery.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "bsp.h"
#include "device_manager.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf51_bitfields.h"
#include "pstorage.h"
#include "softdevice_handler.h"


#define UART_TX_BUF_SIZE                1024         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                1            /**< UART RX buffer size. */

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0            /**< Include or exclude the service_changed characteristic. If excluded, the server's database cannot be changed for the lifetime of the device. */

#define WAKEUP_BUTTON_ID                0            /**< Button used to wake up the application. */
#define BOND_DELETE_ALL_BUTTON_ID       1            /**< Button used to delete all bonded centrals during startup. */
#define CURRENT_TIME_READ_BUTTON_ID     0            /**< Button used to read the current time from the server/central. */

#define DEVICE_NAME                     "Nordic_CTS" /**< Name of the device. Will be included in the advertising data. */
#define APP_ADV_INTERVAL_FAST           0x0028       /**< Fast advertising interval (in units of 0.625 ms). The default value corresponds to 25 ms. */
#define APP_ADV_INTERVAL_SLOW           0x0C80       /**< Slow advertising interval (in units of 0.625 ms). The default value corresponds to 2 seconds. */
#define APP_SLOW_ADV_TIMEOUT            180          /**< The duration of the slow advertising period (in seconds). */
#define APP_FAST_ADV_TIMEOUT            30           /**< The duration of the fast advertising period (in seconds). */

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            (3 + BSP_APP_TIMERS_NUMBER)                 /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                           /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(500, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(1000, UNIT_1_25_MS)           /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory time-out (4 seconds). */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define APP_GPIOTE_MAX_USERS            1                                           /**< Maximum number of users of the GPIOTE handler. */

#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)    /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */
#define SECURITY_REQUEST_DELAY          APP_TIMER_TICKS(4000, APP_TIMER_PRESCALER)  /**< Delay after connection until security request is sent, if necessary (ticks). */

#define SEC_PARAM_TIMEOUT               30                                          /**< Time-out for pairing request or security request (in seconds). */
#define SEC_PARAM_BOND                  1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                           /**< Man In The Middle protection requirement. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                        /**< I/O capabilities. */
#define SEC_PARAM_OOB                   0                                           /**< Out Of Band data availability. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                          /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


static uint16_t m_conn_handle               = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */
static bool     m_whitelist_exists          = false;                   /**< Flag to keep track of whitelist advertising. */
static bool     m_memory_access_in_progress = false;                   /**< Flag to keep track of ongoing operations on persistent memory. */

static ble_db_discovery_t        m_ble_db_discovery;                   /**< Structure used to identify the DB Discovery module. */
static ble_cts_c_t               m_cts;                                /**< Structure to store the data of the current time service. */
static dm_application_instance_t m_app_handle;                         /**< Application identifier allocated by the Device Manager. */
static dm_handle_t               m_peer_handle;                        /**< The peer that is currently connected. */
static ble_gap_adv_params_t      m_adv_params;                         /**< Parameters to be passed to the stack when starting advertising. */

static uint8_t        m_advertising_mode;                              /**< Variable to keep track of when we are advertising. */
static app_timer_id_t m_sec_req_timer_id;                              /**< Security request timer. */

#define SCHED_MAX_EVENT_DATA_SIZE sizeof(app_timer_event_t)            /**< Maximum size of scheduler events. Note that scheduler BLE stack events do not contain any data, as the events are being pulled from the stack in the event handler. */
#define SCHED_QUEUE_SIZE          10                                   /**< Maximum number of events in the scheduler queue. */

typedef enum
{
    BLE_NO_ADVERTISING,   /**< No advertising running. */
    BLE_SLOW_ADVERTISING, /**< Slow advertising running. */
    BLE_FAST_ADVERTISING  /**< Fast advertising running. */
} ble_advertising_mode_t;


static const char * day_of_week[8]    = {"Unknown",
                                         "Monday",
                                         "Tuesday",
                                         "Wednesday",
                                         "Thursday",
                                         "Friday",
                                         "Saturday",
                                         "Sunday"};

static const char * month_of_year[13] = {"Unknown",
                                         "January",
                                         "February",
                                         "March",
                                         "April",
                                         "May",
                                         "June",
                                         "July",
                                         "August",
                                         "September",
                                         "October",
                                         "November",
                                         "December"};


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num  Line number of the failing ASSERT call.
 * @param[in] file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for handling the Current Time Service errors.
 *
 * @param[in]  nrf_error  Error code containing information about what went wrong.
 */
static void current_time_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling UART errors.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}


/**@brief Function for handling the security request timer time-out.
 *
 * @details This function will be called each time the security request timer expires.
 *
 * @param[in] p_context  Pointer used for passing some arbitrary information (context) from the
 *                       app_start_timer() call to the time-out handler.
 */
static void sec_req_timeout_handler(void * p_context)
{
    uint32_t             err_code;
    dm_security_status_t status;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        err_code = dm_security_status_req(&m_peer_handle, &status);
        APP_ERROR_CHECK(err_code);

        // If the link is still not secured by the peer, initiate security procedure.
        if (status == NOT_ENCRYPTED)
        {
            err_code = dm_security_setup_req(&m_peer_handle);
            APP_ERROR_CHECK(err_code);
        }
    }
}


/**@brief Function for handling the Current Time Service errors.
 *
 * @param[in] p_evt  Event received from the Current Time Service client.
 */
static void current_time_print(ble_cts_c_evt_t * p_evt)
{
    printf("\nCurrent Time:\n");
    printf("\nDate:\n");

    printf("\tDay of week   %s\n", day_of_week[p_evt->
                                               params.
                                               current_time.
                                               exact_time_256.
                                               day_date_time.
                                               day_of_week]);

    if (p_evt->params.current_time.exact_time_256.day_date_time.date_time.day == 0)
    {
        printf("\tDay of month  Unknown\n");
    }
    else
    {
        printf("\tDay of month  %i\n",
               p_evt->params.current_time.exact_time_256.day_date_time.date_time.day);
    }

    printf("\tMonth of year %s\n",
           month_of_year[p_evt->params.current_time.exact_time_256.day_date_time.date_time.month]);
    if (p_evt->params.current_time.exact_time_256.day_date_time.date_time.year == 0)
    {
        printf("\tYear          Unknown\n");
    }
    else
    {
        printf("\tYear          %i\n",
               p_evt->params.current_time.exact_time_256.day_date_time.date_time.year);
    }
    printf("\nTime:\n");
    printf("\tHours     %i\n",
           p_evt->params.current_time.exact_time_256.day_date_time.date_time.hours);
    printf("\tMinutes   %i\n",
           p_evt->params.current_time.exact_time_256.day_date_time.date_time.minutes);
    printf("\tSeconds   %i\n",
           p_evt->params.current_time.exact_time_256.day_date_time.date_time.seconds);
    printf("\tFractions %i/256 of a second\n",
           p_evt->params.current_time.exact_time_256.fractions256);

    printf("\nAdjust reason:\n");
    printf("\tDaylight savings %x\n",
           p_evt->params.current_time.adjust_reason.change_of_daylight_savings_time);
    printf("\tTime zone        %x\n",
           p_evt->params.current_time.adjust_reason.change_of_time_zone);
    printf("\tExternal update  %x\n",
           p_evt->params.current_time.adjust_reason.external_reference_time_update);
    printf("\tManual update    %x\n",
           p_evt->params.current_time.adjust_reason.manual_time_update);
}


/**@brief Function for the timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module, making it use the scheduler
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    // Create security request timer.
    err_code = app_timer_create(&m_sec_req_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                sec_req_timeout_handler);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for handling the Current Time Service client events.
 *
 * @details This function will be called for all events in the Current Time Service client that
 *          are passed to the application.
 *
 * @param[in] p_evt Event received from the Current Time Service client.
 */
static void on_cts_c_evt(ble_cts_c_t * p_cts, ble_cts_c_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_CTS_C_EVT_DISCOVERY_COMPLETE:
            printf("Current Time Service discovered on server.\n");
            break;

        case BLE_CTS_C_EVT_SERVICE_NOT_FOUND:
            printf("Current Time Service not found on server.\n");
            break;

        case BLE_CTS_C_EVT_DISCONN_COMPLETE:
            printf("Disconnect Complete.\n");
            break;

        case BLE_CTS_C_EVT_CURRENT_TIME:
            printf("Current Time received.\n");
            current_time_print(p_evt);
            break;

        case BLE_CTS_C_EVT_INVALID_TIME:
            printf("Invalid Time received.\n");
            break;

        default:
            break;
    }
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(uint8_t adv_flags)
{
    uint32_t      err_code;
    ble_advdata_t advdata;

    // YOUR_JOB: Use UUIDs for service(s) used in your application.
    ble_uuid_t adv_uuids[] = {{BLE_UUID_CURRENT_TIME_SERVICE, BLE_UUID_TYPE_BLE}};

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    m_advertising_mode = BLE_NO_ADVERTISING;

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = adv_flags;
    advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = adv_uuids;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t         err_code;
    ble_cts_c_init_t cts_init_obj;

    cts_init_obj.evt_handler   = on_cts_c_evt;
    cts_init_obj.error_handler = current_time_error_handler;
    err_code                   = ble_cts_c_init(&m_cts, &cts_init_obj);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Connection Parameters module.
 *
 * @details This function will be called for all events in the Connection Parameters module that
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t            err_code;
    ble_gap_whitelist_t whitelist;

    memset(&whitelist, 0, sizeof(ble_gap_whitelist_t));
    ble_gap_addr_t * p_whitelist_addr[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
    ble_gap_irk_t  * p_whitelist_irk[BLE_GAP_WHITELIST_IRK_MAX_COUNT];
    uint32_t         count;

    // Verify if there is any flash access pending, if there is, delay starting advertising until
    // it's complete.
    err_code = pstorage_access_status_get(&count);
    APP_ERROR_CHECK(err_code);

    if (count != 0)
    {
        m_memory_access_in_progress = true;
        return;
    }

    whitelist.addr_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
    whitelist.irk_count  = BLE_GAP_WHITELIST_IRK_MAX_COUNT;
    whitelist.pp_addrs   = p_whitelist_addr;
    whitelist.pp_irks    = p_whitelist_irk;

    err_code = dm_whitelist_create(&m_app_handle, &whitelist);
    APP_ERROR_CHECK(err_code);

    if ((whitelist.addr_count != 0) || (whitelist.irk_count != 0))
    {
        m_whitelist_exists       = true;
        m_adv_params.fp          = BLE_GAP_ADV_FP_FILTER_CONNREQ;
        m_adv_params.p_whitelist = &whitelist;
    }

    if (m_advertising_mode == BLE_NO_ADVERTISING)
    {
        m_advertising_mode = BLE_FAST_ADVERTISING;
    }
    else
    {
        m_advertising_mode = BLE_SLOW_ADVERTISING;
    }

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    m_adv_params.p_peer_addr = NULL; // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;

    if (m_advertising_mode == BLE_FAST_ADVERTISING)
    {
        m_adv_params.interval = APP_ADV_INTERVAL_FAST;
        m_adv_params.timeout  = APP_FAST_ADV_TIMEOUT;
        err_code              = bsp_indication_set(BSP_INDICATE_ADVERTISING);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        m_adv_params.interval = APP_ADV_INTERVAL_SLOW;
        m_adv_params.timeout  = APP_SLOW_ADV_TIMEOUT;
        err_code              = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
        APP_ERROR_CHECK(err_code);
    }

    if (m_whitelist_exists == true)
    {
        err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
        APP_ERROR_CHECK(err_code);
    }

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the application's system events.
 *
 * @param[in] sys_evt  System event.
 */
static void on_sys_evt(uint32_t sys_evt)
{
    switch (sys_evt)
    {
        case NRF_EVT_FLASH_OPERATION_SUCCESS:
            // fall through.
        case NRF_EVT_FLASH_OPERATION_ERROR:

            if (m_memory_access_in_progress)
            {
                m_memory_access_in_progress = false;
                advertising_start();
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the system event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    on_sys_evt(sys_evt);
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in] p_evt  Data associated to the Device Manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const * p_handle,
                                           dm_event_t const  * p_event,
                                           ret_code_t        event_result)
{
    uint32_t err_code;

    APP_ERROR_CHECK(event_result);

    switch (p_event->event_id)
    {
        case DM_EVT_CONNECTION:
            m_peer_handle = (*p_handle);
            err_code      = app_timer_start(m_sec_req_timer_id, SECURITY_REQUEST_DELAY, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case DM_EVT_LINK_SECURED:
            err_code = ble_db_discovery_start(&m_ble_db_discovery,
                                              p_event->event_param.p_gap_param->conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;

    }
    return NRF_SUCCESS;
}


/**@brief Function for the Device Manager initialization.
 */
static void device_manager_init(void)
{
    uint32_t               err_code;
    dm_init_param_t        init_data;
    dm_application_param_t register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    // Clear all bonded centrals if the Bonds Delete button is pushed.
    err_code = bsp_button_is_pressed(BOND_DELETE_ALL_BUTTON_ID, &(init_data.clear_persistent_data));
    APP_ERROR_CHECK(err_code);
    if (err_code == NRF_ERROR_INVALID_PARAM)
    {
        init_data.clear_persistent_data = false;
    }

    err_code = dm_init(&init_data);
    APP_ERROR_CHECK(err_code);

    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_CLI_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the application's BLE stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);

            m_advertising_mode = BLE_NO_ADVERTISING;
            m_conn_handle      = p_ble_evt->evt.gap_evt.conn_handle;

            // Start handling button presses
            err_code = bsp_buttons_enable(1 << CURRENT_TIME_READ_BUTTON_ID);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            err_code = bsp_buttons_enable(BSP_BUTTONS_NONE);
            APP_ERROR_CHECK(err_code);
            advertising_start();
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
            {
                if (m_advertising_mode == BLE_FAST_ADVERTISING)
                {
                    // If we time out from 'fast' advertising, we will restart advertising in 'slow' advertising mode. 
                    advertising_start();
                }
                else
                {
                    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
                    APP_ERROR_CHECK(err_code);

                    m_advertising_mode = BLE_NO_ADVERTISING;

                    err_code = bsp_buttons_enable(  (1 << WAKEUP_BUTTON_ID)
                                                  | (1 << BOND_DELETE_ALL_BUTTON_ID));
                    APP_ERROR_CHECK(err_code);

                    // Go to system-off mode. This function will not return; wakeup will cause a reset.
                    err_code = sd_power_system_off();
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for handling button events.
 *
 * @param[in] event  Event generated by button press.
 */
static void button_event_handler(bsp_event_t event)
{
    uint32_t err_code;

    switch (event)
    {
        case BSP_EVENT_KEY_0:
            err_code = ble_cts_c_current_time_read(&m_cts);
            if (err_code == NRF_ERROR_NOT_FOUND)
            {
                printf("Current Time Service is not discovered.\r\n");
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    dm_ble_evt_handler(p_ble_evt);
    ble_db_discovery_on_ble_evt(&m_ble_db_discovery, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_cts_c_on_ble_evt(&m_cts, p_ble_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);

#ifdef S110
    // Enable BLE stack
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
#endif

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}


/**@brief Function for initializing the GPIOTE handler module.
 */
static void gpiote_init(void)
{
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
}


/**@brief Function for the power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Board Support Package (BSP) module.
 */
static void bsp_module_init(void)
{
    uint32_t err_code;

    // Note: If the only use of buttons is to wake up, bsp_event_handler can be NULL.
    err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS,
                        APP_TIMER_TICKS(100, APP_TIMER_PRESCALER),
                        button_event_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the UART.
 */
static void uart_init(void)
{
    uint32_t                     err_code;
    const app_uart_comm_params_t comm_params =
    {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_ENABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud38400
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_error_handle,
                       APP_IRQ_PRIORITY_LOW,
                       err_code);
    APP_ERROR_CHECK(err_code);
}


/**
 * @brief Database discovery collector initialization.
 */
static void db_discovery_init(void)
{
    uint32_t err_code = ble_db_discovery_init();

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
 */
int main(void)
{
    // Initialize
    app_trace_init();
    timers_init();
    uart_init();
    printf("CTS Start!\n");
    gpiote_init();
    bsp_module_init();
    ble_stack_init();
    device_manager_init();
    db_discovery_init();
    scheduler_init();
    gap_params_init();
    advertising_init(BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE);
    services_init();
    conn_params_init();

    // Start execution
    advertising_start();

    // Enter main loop
    for (;;)
    {
        app_sched_execute();
        power_manage();
    }
}


/**
 * @}
 */
