/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 */

/** @file
 *
 * @defgroup ble_sdk_apple_notification_main main.c
 * @{
 * @ingroup ble_sdk_app_apple_notification
 * @brief Apple Notification Client Sample Application main file.  - Disclaimer: This module (Apple Notification Center Service) can and will be changed at any time by either Apple or Nordic Semiconductor ASA.
 *
 * This file contains the source code for a sample application using the Apple Notification Profile 
 * Client. This application uses the @ref srvlib_conn_params module.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ble_ancs_c.h"
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble_hci.h"
#include "ble_gap.h"
#include "ble_advdata.h"
#include "nrf_gpio.h"
#include "ble_srv_common.h"
#include "ble_conn_params.h"
#include "nrf51_bitfields.h"
#include "device_manager.h"
#include "app_gpiote.h"
#include "app_button.h"
#include "app_timer.h"
#include "pstorage.h"
#include "nrf_soc.h"
#include "bsp.h"
#include "softdevice_handler.h"
#include "app_trace.h"

#if BUTTONS_NUMBER<2
#error "Not enough resources on board"
#endif

#define UART_TX_BUF_SIZE 256                                                                 /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                                                                   /**< UART RX buffer size. */

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                                    /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define MAX_CHARACTERS_PER_LINE         16                                                   /**< Maximum characters that are visible in one line on nRF6350 display */
#define DISPLAY_BUFFER_SIZE             ANCS_ATTRIBUTE_DATA_MAX                              /**< Size of LCD display buffer */

#define BOND_DELETE_ALL_WAKEUP_BUTTON_ID 0                                                   /**< Button used for deleting all bonded masters/services during startup and to wake up */
#define DISPLAY_MESSAGE_BUTTON_ID        1                                                   /**< Button used to display message contents */

#define DEVICE_NAME                     "ANCC"                                               /**< Name of device. Will be included in the advertising data. */
#define APP_ADV_INTERVAL                40                                                   /**< The advertising interval (in units of 0.625 ms. This value corresponds to 25 ms). */
#define APP_ADV_INTERVAL_SLOW           3200                                                 /**< Slow advertising interval (in units of 0.625 ms. This value corresponds to 2 seconds). */
#define APP_ADV_TIMEOUT_IN_SECONDS      180                                                  /**< The advertising timeout in units of seconds. */
#define ADV_INTERVAL_FAST_PERIOD        30                                                   /**< The duration of the fast advertising period (in seconds). */

#define APP_TIMER_PRESCALER             0                                                    /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            (2+BSP_APP_TIMERS_NUMBER)                            /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                                    /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(500, UNIT_1_25_MS)                     /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(1000, UNIT_1_25_MS)                    /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   0                                                    /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)                      /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)           /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)          /**< Time between each call to sd_ble_gap_conn_param_update after the first (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                                    /**< Number of attempts before giving up the connection parameter negotiation. */

#define MESSAGE_BUFFER_SIZE             18                                                   /**< Size of buffer holding optional messages in notifications. */

#define APP_GPIOTE_MAX_USERS            1                                                    /**< Maximum number of users of the GPIOTE handler. */

#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)             /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define SEC_PARAM_BOND                  1                                                    /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                                    /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                                 /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                                    /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                                    /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                                   /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                                           /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

typedef enum
{
    BLE_NO_ADVERTISING,                                                                      /**< No advertising running. */
    BLE_SLOW_ADVERTISING,                                                                    /**< Slow advertising running. */
    BLE_FAST_ADVERTISING                                                                     /**< Fast advertising running. */
} ble_advertising_mode_t;


static const char *lit_catid[] =
{
    "Other",
    "IncomingCall",
    "MissedCall",
    "VoiceMail",
    "Social",
    "Schedule",
    "Email",
    "News",
    "HealthAndFitness",
    "BusinessAndFinance",
    "Location",
    "Entertainment"
};

static const char *lit_eventid[] =
{
    "Added",
    "Modified",
    "Removed"
};

static ble_ancs_c_t                     m_ancs_c;                                            /**< Structure used to identify the Apple Notification Service Client. */
static uint8_t                          m_apple_message_buffer[MESSAGE_BUFFER_SIZE];         /**< Message buffer for optional notify messages. */

static ble_gap_adv_params_t             m_adv_params;                                        /**< Parameters to be passed to the stack when starting advertising. */
static ble_advertising_mode_t           m_advertising_mode;                                  /**< Variable to keep track of when we are advertising. */



static char                             display_title[DISPLAY_BUFFER_SIZE];
static char                             display_message[DISPLAY_BUFFER_SIZE];
static uint8_t                          m_ancs_uuid_type;
static dm_application_instance_t        m_app_handle;                                        /**< Application identifier allocated by device manager. */
static dm_handle_t                      m_peer_handle;                                       /**< Identifes the peer that is currently connected. */

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

void uart_error_handle(app_uart_evt_t * p_event)
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

/**@brief Function for setup of apple notifications in central.
 *
 * @details This function will be called when a successful connection has been established.
 */
static void apple_notification_setup(void)
{
    uint32_t err_code;

    err_code = ble_ancs_c_enable_notif_notification_source(&m_ancs_c);
    APP_ERROR_CHECK(err_code);

    err_code = ble_ancs_c_enable_notif_data_source(&m_ancs_c);
    APP_ERROR_CHECK(err_code);
}

/**@brief Display an iOS notification
 *
 */
void evt_ios_notification(ble_ancs_c_evt_ios_notification_t *p_notice)
{
    char buffer[DISPLAY_BUFFER_SIZE];

    printf("Category: %s\n",lit_catid[p_notice->category_id]);
    
    sprintf(buffer, "%s %02x%02x%02x%02x",
        lit_eventid[p_notice->event_id], 
        p_notice->notification_uid[0], p_notice->notification_uid[1], 
        p_notice->notification_uid[2], p_notice->notification_uid[3]);

    printf("Notification: %s\n",buffer);
}

/**@brief Display iOS notification message contents
 *
 */
static void display_ios_message(void)
{
    printf("Title: %s\n",display_title);
    printf("Message: %s\n",display_message);
}

/**@brief Copy notification attribute data to buffer
 *
 */
static void evt_notif_attribute(ble_ancs_c_evt_notif_attribute_t *p_attr)
{
    if (p_attr->attribute_id == BLE_ANCS_NOTIFICATION_ATTRIBUTE_ID_TITLE)
    {
        memcpy(display_title, p_attr->data, p_attr->attribute_len);
    }
    else if (p_attr->attribute_id == BLE_ANCS_NOTIFICATION_ATTRIBUTE_ID_MESSAGE)
    {
        memcpy(display_message, p_attr->data, p_attr->attribute_len);
    }
}


/**@brief Function for handling the Apple Notification Service Client.
 *
 * @details This function will be called for all events in the Apple Notification Client which
 *          are passed to the application.
 *
 * @param[in]   p_evt   Event received from the Apple Notification Service Client.
 */
static void on_ancs_c_evt(ble_ancs_c_evt_t * p_evt)
{
    uint32_t err_code = NRF_SUCCESS;

    switch (p_evt->evt_type)
    {
        case BLE_ANCS_C_EVT_DISCOVER_COMPLETE:
            err_code = dm_security_setup_req(&m_peer_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ANCS_C_EVT_IOS_NOTIFICATION:
            evt_ios_notification(&p_evt->data.notification);
            break;

        case BLE_ANCS_C_EVT_NOTIF_ATTRIBUTE:
            evt_notif_attribute(&p_evt->data.attribute);
            break;

        default:
            //No implementation needed
            break;
    }
}


/**@brief Function for the GAP initialization.
 *
 * @details This function shall be used to setup all the necessary GAP (Generic Access Profile)
 *          parameters of the device. It also sets the permissions and appearance.
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


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    ble_uuid_t    ancs_uuid;
    
    ancs_uuid.uuid = ((ble_ancs_base_uuid128.uuid128[12]) | (ble_ancs_base_uuid128.uuid128[13] << 8));
    ancs_uuid.type = m_ancs_uuid_type;    
    
    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));
    
    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = flags;
    advdata.uuids_complete.uuid_cnt = 0;
    advdata.uuids_complete.p_uuids  = NULL;
    advdata.uuids_solicited.uuid_cnt = 1;
    advdata.uuids_solicited.p_uuids  = &ancs_uuid;    
    
    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code;

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
    m_adv_params.p_peer_addr = NULL;                           // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;

    if (m_advertising_mode == BLE_FAST_ADVERTISING)
    {
        m_adv_params.interval = APP_ADV_INTERVAL;
        m_adv_params.timeout  = ADV_INTERVAL_FAST_PERIOD;
    }
    else
    {
        m_adv_params.interval = APP_ADV_INTERVAL_SLOW;
        m_adv_params.timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
    }

    err_code = sd_ble_gap_adv_start(&m_adv_params);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Apple Notification Service Client errors.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void apple_notification_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
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
    cp_init.disconnect_on_fail             = true;
    cp_init.evt_handler                    = NULL;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in]   p_evt   Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const    * p_handle,
                                           dm_event_t const     * p_event,
                                           ret_code_t           event_result)
{
    APP_ERROR_CHECK(event_result);
    ble_ancs_c_on_device_manager_evt(&m_ancs_c, p_handle, p_event);
    switch(p_event->event_id)
    {
        case DM_EVT_CONNECTION:
            m_peer_handle = (*p_handle);
            break;
    }
    return NRF_SUCCESS;
}


/**@brief Function for the Device Manager initialization.
 */
static void device_manager_init(void)
{
    uint32_t                err_code;
    dm_init_param_t         init_data;
    dm_application_param_t  register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    // Clear all bonded centrals if the "delete all bonds" button is pushed.
    err_code = bsp_button_is_pressed(BOND_DELETE_ALL_WAKEUP_BUTTON_ID,&(init_data.clear_persistent_data));
    APP_ERROR_CHECK(err_code);

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
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}



/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t        err_code = NRF_SUCCESS;
    static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
    
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);

            err_code = bsp_buttons_enable((1<<DISPLAY_MESSAGE_BUTTON_ID));
            APP_ERROR_CHECK(err_code);
            
            m_advertising_mode = BLE_NO_ADVERTISING;
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;
        
        case BLE_GAP_EVT_AUTH_STATUS:
            apple_notification_setup();
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_IDLE);
            APP_ERROR_CHECK(err_code);

            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            // Stop detecting button presses when not connected.
            err_code = bsp_buttons_enable(BSP_BUTTONS_NONE);
            APP_ERROR_CHECK(err_code);

            err_code = ble_ans_c_service_store();
            APP_ERROR_CHECK(err_code);

            advertising_start();
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
            {
                if (m_advertising_mode == BLE_FAST_ADVERTISING)
                {
                    advertising_start();
                }
                else
                {
                    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
                    APP_ERROR_CHECK(err_code);

                    m_advertising_mode = BLE_NO_ADVERTISING;
                    
                    err_code = bsp_buttons_enable( (1 << BOND_DELETE_ALL_WAKEUP_BUTTON_ID) );
                    APP_ERROR_CHECK(err_code);

                    // Go to system-off mode (function will not return; wakeup will cause a reset).
                    err_code = sd_power_system_off();
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        case BLE_GATTC_EVT_TIMEOUT:
        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server and Client timeout events.
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling button events.
 *
 * @param[in]   event   Event generated by button pressed.
 */
static void button_event_handler(bsp_event_t event)
{
    switch (event)
    {
        case BSP_EVENT_KEY_1:
            display_ios_message();
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
    dm_ble_evt_handler(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_ancs_c_on_ble_evt(&m_ancs_c, p_ble_evt);
    on_ble_evt(p_ble_evt);
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
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

    // Enable BLE stack 
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // Register with the SoftDevice handler module for System events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

static void service_add(void)
{
    ble_ancs_c_init_t ancs_init_obj;
    ble_uuid_t        service_uuid;
    uint32_t          err_code;
    bool              services_delete;

    err_code = sd_ble_uuid_vs_add(&ble_ancs_base_uuid128, &m_ancs_uuid_type);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_uuid_vs_add(&ble_ancs_cp_base_uuid128, &service_uuid.type);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_uuid_vs_add(&ble_ancs_ns_base_uuid128, &service_uuid.type);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_uuid_vs_add(&ble_ancs_ds_base_uuid128, &service_uuid.type);
    APP_ERROR_CHECK(err_code);

    memset(&ancs_init_obj, 0, sizeof(ancs_init_obj));
    memset(m_apple_message_buffer, 0, MESSAGE_BUFFER_SIZE);

    ancs_init_obj.evt_handler         = on_ancs_c_evt;
    ancs_init_obj.message_buffer_size = MESSAGE_BUFFER_SIZE;
    ancs_init_obj.p_message_buffer    = m_apple_message_buffer;
    ancs_init_obj.error_handler       = apple_notification_error_handler;

    err_code = ble_ancs_c_init(&m_ancs_c, &ancs_init_obj);
    APP_ERROR_CHECK(err_code);
    
    // Clear all discovered and stored services if the  "delete all bonds" button is pushed.
    err_code = bsp_button_is_pressed(BOND_DELETE_ALL_WAKEUP_BUTTON_ID,&(services_delete));
    APP_ERROR_CHECK(err_code);

    if (services_delete)
    {
        err_code = ble_ans_c_service_delete();
        APP_ERROR_CHECK(err_code);
    }

    err_code = ble_ans_c_service_load(&m_ancs_c);
    APP_ERROR_CHECK(err_code);    
}


/**@brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
    // Initialize.
    app_trace_init();

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
                     
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
    ble_stack_init();
    err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS, APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), button_event_handler);
    APP_ERROR_CHECK(err_code);
    printf("BLE ANCS\n");
    device_manager_init();
    gap_params_init();
    service_add();
    advertising_init();
    conn_params_init();

    // Start execution.
    advertising_start();

    // Enter main loop.
    for (;;)
    {
        power_manage();
    }

}

/**
 * @}
 */

