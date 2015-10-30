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
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "app_trace.h"
#include "app_timer_appsh.h"
#include "app_button.h"
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
#include "lwip/timers.h"
#include "nrf_platform_port.h"
#include "app_util_platform.h"
#include "iot_timer.h"
#include "coap_api.h"
#include "coap_observe_api.h"


#define DEVICE_NAME                     "LwIPCOAPServerObs"                                   /**< Device name used in BLE undirected advertisement. */

#define LED_ONE                         BSP_LED_0_MASK
#define LED_TWO                         BSP_LED_1_MASK
#define LED_THREE                       BSP_LED_2_MASK
#define LED_FOUR                        BSP_LED_3_MASK

#define BUTTON_ONE                      BSP_BUTTON_0

#define COMMAND_OFF                     0x30
#define COMMAND_ON                      0x31
#define COMMAND_TOGGLE                  0x32

#define APP_TIMER_PRESCALER             NRF51_DRIVER_TIMER_PRESCALER                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            2                                                     /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         4                                                     /**< Size of timer operation queues. */

#define APP_ADV_TIMEOUT                 0                                                     /**< Time for which the device must be advertising in non-connectable mode (in seconds). 0 disables timeout. */
#define APP_ADV_ADV_INTERVAL            MSEC_TO_UNITS(333, UNIT_0_625_MS)                     /**< The advertising interval. This value can vary between 100ms to 10.24s). */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)              /**< Button debounce interval. */

#define LWIP_SYS_TICK_MS                100                                                   /**< Interval for timer used as trigger to send. */
#define LED_BLINK_INTERVAL_MS           300                                                   /**< LED blinking interval. */
#define COAP_TICK_INTERVAL_MS           1000                                                  /**< Interval between periodic callbacks to CoAP module. */

#define APP_RTR_SOLICITATION_DELAY      500                                                   /**< Time before host sends an initial solicitation in ms. */

#define DEAD_BEEF                       0xDEADBEEF                                            /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
#define MAX_LENGTH_FILENAME             128                                                   /**< Max length of filename to copy for the debug error handler. */

#define OBSERVE_NOTIFY_DELTA_MAX_AGE    2                                                     /**< Number of seconds prior to a max-age timeout which an updated state of the observed value should be sent to the observers. */

#define APP_DISABLE_LOGS                0                                                     /**< Disable debug trace in the application. */

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
    LEDS_IPV6_IF_UP,
} display_state_t;

eui64_t                       eui64_local_iid;                                                /**< Local EUI64 value that is used as the IID for SLAAC. */
static ble_gap_addr_t         m_local_ble_addr;                                               /**< Local BLE address. */
static ble_gap_adv_params_t   m_adv_params;                                                   /**< Parameters to be passed to the stack when starting advertising. */
static app_timer_id_t         m_iot_timer_tick_src_id;                                        /**< System Timer used to service CoAP and LWIP periodically. */
static uint8_t                m_well_known_core[100];
static display_state_t        m_display_state = LEDS_INACTIVE;                                /**< Board LED display state. */
static const char             m_lights_name[] = "lights";
static const char             m_led3_name[]   = "led3";

static coap_resource_t        m_led3;
static uint32_t               m_observer_sequence_num = 0;

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
    APPL_LOG("[** ASSERT **]: Error 0x%08lX, Line %ld, File %s\r\n", error_code, line_num, p_file_name);

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
    LEDS_CONFIGURE((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));

    // Turn leds off.
    LEDS_OFF((LED_ONE | LED_TWO | LED_THREE | LED_FOUR));
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


/**@brief Function for handling the timer used to trigger TCP actions.
 *
 * @details This function is used to trigger TCP connection request and to send data on the TCP port.
 *
 * @param[in]   p_context   Pointer used for passing context. No context used in this application.
 */
static void app_lwip_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    sys_check_timeouts();
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
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create a sys timer.
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

    IPV6_EUI64_CREATE_FROM_EUI48(eui64_local_iid.identifier,
                                 m_local_ble_addr.addr,
                                 m_local_ble_addr.addr_type);

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


static void led_value_get(coap_content_type_t content_type, char ** str)
{
    if (content_type == COAP_CT_APP_JSON)
    {
        static char response_str_true[15] = {"{\"led3\": True}\0"};
        static char response_str_false[16] = {"{\"led3\": False}\0"};
        if((bool)LED_IS_ON(LED_THREE) == true)
        {
            *str = response_str_true;
        }
        else
        {
            *str = response_str_false;
        }
    }
    // For all other use plain text
    else
    {
        static char response_str[2];
        memset(response_str, '\0', sizeof(response_str));
        sprintf(response_str, "%d", (bool)LED_IS_ON(LED_THREE));
        *str = response_str;
    }
}

static void observer_con_message_callback(uint32_t status, void * arg, coap_message_t * p_response)
{
    uint32_t err_code;
    switch (status)
    {
        case COAP_TRANSMISSION_RESET_BY_PEER:
            {
            }
        // Fallthrough.
        case COAP_TRANSMISSION_TIMEOUT:
            {
                coap_observer_t * p_observer = (coap_observer_t *)arg;

                // Remove observer from its list.
                uint32_t handle;
                err_code = coap_observe_server_search(&handle, &p_observer->remote, p_observer->p_resource_of_interest);
                APP_ERROR_CHECK(err_code);

                err_code = coap_observe_server_unregister(handle);
                APP_ERROR_CHECK(err_code);
            }
            break;

        default:
            {
                // The CON message went find.
            }
            break;
    }
}

static void notify_all_led3_subscribers(coap_msg_type_t type)
{
    // Fetch all observers which are subscribed to this resource.
    // Then send an update value too each observer.
    coap_observer_t * p_observer = NULL;
    while (coap_observe_server_next_get(&p_observer, p_observer, &m_led3) == NRF_SUCCESS)
    {
        // Generate a message.
        coap_message_conf_t response_config;
        memset(&response_config, 0, sizeof(coap_message_conf_t));

        response_config.type              = type;
        response_config.code              = COAP_CODE_205_CONTENT;
        response_config.response_callback = observer_con_message_callback;

        // Copy token.
        memcpy(&response_config.token[0], &p_observer->token[0], p_observer->token_len);
        // Copy token length.
        response_config.token_len = p_observer->token_len;
        response_config.port.port_number = COAP_SERVER_PORT;

        coap_message_t * p_response;
        uint32_t err_code = coap_message_new(&p_response, &response_config);
        APP_ERROR_CHECK(err_code);

        // Set custom misc. argument.
        p_response->p_arg = p_observer;

        err_code = coap_message_remote_addr_set(p_response, &p_observer->remote);
        APP_ERROR_CHECK(err_code);

        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_OBSERVE, m_observer_sequence_num++);
        APP_ERROR_CHECK(err_code);

        err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, m_led3.expire_time);
        APP_ERROR_CHECK(err_code);

        char * response_str;
        led_value_get(p_observer->ct, &response_str);
        err_code = coap_message_payload_set(p_response, response_str, strlen(response_str));
        APP_ERROR_CHECK(err_code);

        uint32_t msg_handle;
        err_code = coap_message_send(&msg_handle, p_response);
        APP_ERROR_CHECK(err_code);

        err_code = coap_message_delete(p_response);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for catering CoAP module with periodic time ticks.
*/
static void app_coap_time_tick(iot_timer_time_in_ms_t wall_clock_value)
{
    // Pass a tick to coap in order to re-transmit any pending messages.
    (void)coap_time_tick();

    // Check if any of the observers needs an update.
    static uint32_t msg_count = 0;

    if (m_led3.expire_time <= (0 + OBSERVE_NOTIFY_DELTA_MAX_AGE))
    {
        m_led3.expire_time = m_led3.max_age;

        if (msg_count % 4 == 0)
        {
            notify_all_led3_subscribers(COAP_TYPE_CON);
        }
        else
        {
            notify_all_led3_subscribers(COAP_TYPE_NON);
        }

        msg_count++;
    }
    else
    {
        m_led3.expire_time--;
    }
}


void well_known_core_callback(coap_resource_t * p_resource, coap_message_t * p_request)
{
    coap_message_conf_t response_config;
    memset (&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code = COAP_CODE_205_CONTENT;
    // Copy message ID.
    response_config.id = p_request->header.id;

    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t err_code = coap_message_new(&p_response, &response_config);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_remote_addr_set(p_response, &p_request->remote);
    APP_ERROR_CHECK(err_code);

    memcpy(&p_response->remote, &p_request->remote, sizeof(coap_remote_t));

    err_code = coap_message_opt_uint_add(p_response, COAP_OPT_CONTENT_FORMAT, \
                                         COAP_CT_APP_LINK_FORMAT);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_payload_set(p_response, m_well_known_core, \
                                        strlen((char *)m_well_known_core));
    APP_ERROR_CHECK(err_code);

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_delete(p_response);
    APP_ERROR_CHECK(err_code);
}


static void led3_callback(coap_resource_t * p_resource, coap_message_t * p_request)
{
    coap_message_conf_t response_config;
    memset (&response_config, 0, sizeof(coap_message_conf_t));

    if (p_request->header.type == COAP_TYPE_NON)
    {
        response_config.type = COAP_TYPE_NON;
    }
    else if (p_request->header.type == COAP_TYPE_CON)
    {
        response_config.type = COAP_TYPE_ACK;
    }

    // PIGGY BACKED RESPONSE
    response_config.code = COAP_CODE_405_METHOD_NOT_ALLOWED;
    // Copy message ID.
    response_config.id = p_request->header.id;

    // Copy token.
    memcpy(&response_config.token[0], &p_request->token[0], p_request->header.token_len);
    // Copy token length.
    response_config.token_len = p_request->header.token_len;

    coap_message_t * p_response;
    uint32_t err_code = coap_message_new(&p_response, &response_config);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_remote_addr_set(p_response, &p_request->remote);
    APP_ERROR_CHECK(err_code);

    // Handle request.
    switch (p_request->header.code)
    {
        case COAP_CODE_GET:
        {
            p_response->header.code = COAP_CODE_205_CONTENT;
            
            // Select the first common content type between the resource and the CoAP client.
            coap_content_type_t ct_to_use;
            err_code = coap_message_ct_match_select(&ct_to_use, p_request, p_resource);
            if (err_code != NRF_SUCCESS)
            {
                // None of the accepted content formats are supported in this resource endpoint.
                p_response->header.code = COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT;
                p_response->header.type = COAP_TYPE_RST;
            }
            else
            {
                if (coap_message_opt_present(p_request, COAP_OPT_OBSERVE) == NRF_SUCCESS)
                {
                    // Locate the option and check the value.
                    uint32_t observe_option = 0;

                    uint8_t index;
                    for (index = 0; index < p_request->options_count; index++)
                    {
                        if (p_request->options[index].number == COAP_OPT_OBSERVE)
                        {
                            err_code = coap_opt_uint_decode(&observe_option,
                                                                     p_request->options[index].length,
                                                                     p_request->options[index].p_data);
                            if (err_code != NRF_SUCCESS)
                            {
                               APP_ERROR_CHECK(err_code);
                            }
                            break;
                        }
                    }

                    if (observe_option == 0)
                    {                        
                        // Register observer, and if successful, add the Observe option in the reply.
                        uint32_t handle;
                        coap_observer_t observer;
                        
                        // Set the token length.
                        observer.token_len              = p_request->header.token_len;
                        // Set the resource of interest.
                        observer.p_resource_of_interest = p_resource;
                        // Set the remote.
                        memcpy(&observer.remote, &p_request->remote, sizeof(coap_remote_t));
                        // Set the token.
                        memcpy(observer.token, p_request->token, observer.token_len);

                        // Set the content format to be used for subsequent notifications.
                        observer.ct = ct_to_use;
                        
                        err_code = coap_observe_server_register(&handle, &observer);
                        if (err_code == NRF_SUCCESS)
                        {
                            err_code = coap_message_opt_uint_add(p_response, COAP_OPT_OBSERVE, m_observer_sequence_num++);
                            APP_ERROR_CHECK(err_code);

                            err_code = coap_message_opt_uint_add(p_response, COAP_OPT_MAX_AGE, p_resource->expire_time);
                            APP_ERROR_CHECK(err_code);
                        }
                        // If the registration of the observer could not be done, handle this as a normal message.
                    }
                    else
                    {
                        uint32_t handle;
                        err_code = coap_observe_server_search(&handle, &p_request->remote, p_resource);
                        if (err_code == NRF_SUCCESS)
                        {
                            err_code = coap_observe_server_unregister(handle);
                            APP_ERROR_CHECK(err_code);
                        }
                    }
                }
                
                // Set response payload to the actual LED state.
                char * response_str;
                led_value_get(ct_to_use, &response_str);
                err_code = coap_message_payload_set(p_response, response_str, strlen(response_str));
                APP_ERROR_CHECK(err_code);
            }
            break;
        }

        case COAP_CODE_PUT:
        {
            p_response->header.code = COAP_CODE_204_CHANGED;

            // Change LED state according to request. 
            switch (p_request->p_payload[0])
            {
                case COMMAND_ON:
                {
                    LEDS_ON(LED_THREE);
                    break;
                }
                case COMMAND_OFF:
                {
                    LEDS_OFF(LED_THREE);
                    break;
                }
                case COMMAND_TOGGLE:
                {
                    LEDS_INVERT(LED_THREE);
                    break;
                }
                default:
                {
                    p_response->header.code = COAP_CODE_400_BAD_REQUEST;
                    break;
                }
            }
            break;
        }

        default:
        {
            p_response->header.code = COAP_CODE_405_METHOD_NOT_ALLOWED;
            break;
        }
    }

    uint32_t msg_handle;
    err_code = coap_message_send(&msg_handle, p_response);
    APP_ERROR_CHECK(err_code);

    err_code = coap_message_delete(p_response);
    APP_ERROR_CHECK(err_code);

    if (p_request->header.code == COAP_CODE_PUT)
    {
        notify_all_led3_subscribers(COAP_TYPE_NON);
    }
}



static void coap_endpoints_init(void)
{
    uint32_t err_code;

    static coap_resource_t root;
    err_code = coap_resource_create(&root, "/");
    APP_ERROR_CHECK(err_code);

    static coap_resource_t well_known;
    err_code = coap_resource_create(&well_known, ".well-known");
    APP_ERROR_CHECK(err_code);
    err_code = coap_resource_child_add(&root, &well_known);
    APP_ERROR_CHECK(err_code);

    static coap_resource_t core;
    err_code = coap_resource_create(&core, "core");
    APP_ERROR_CHECK(err_code);

    core.permission = COAP_PERM_GET;
    core.callback   = well_known_core_callback;

    err_code = coap_resource_child_add(&well_known, &core);
    APP_ERROR_CHECK(err_code);

    static coap_resource_t lights;
    err_code = coap_resource_create(&lights, m_lights_name);
    APP_ERROR_CHECK(err_code);

    err_code = coap_resource_child_add(&root, &lights);
    APP_ERROR_CHECK(err_code);

    err_code = coap_resource_create(&m_led3, m_led3_name);
    APP_ERROR_CHECK(err_code);

    m_led3.permission      = (COAP_PERM_GET | COAP_PERM_PUT | COAP_PERM_OBSERVE);
    m_led3.callback        = led3_callback;
    m_led3.ct_support_mask = COAP_CT_MASK_APP_JSON | COAP_CT_MASK_PLAIN_TEXT;
    m_led3.max_age         = 15;

    err_code = coap_resource_child_add(&lights, &m_led3);
    APP_ERROR_CHECK(err_code);

    uint16_t size = sizeof(m_well_known_core);
    err_code = coap_resource_well_known_generate(m_well_known_core, &size);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling button events.
 *
 * @param[in]   pin_no         The pin number of the button pressed.
 * @param[in]   button_action  The action performed on button.
 */
static void button_event_handler(uint8_t pin_no, uint8_t button_action)
{    
    if (button_action == APP_BUTTON_PUSH)
    {        
        if (pin_no == BUTTON_ONE)
        {
            LEDS_INVERT(LED_THREE);
            notify_all_led3_subscribers(COAP_TYPE_NON);
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
        {BUTTON_ONE, false, BUTTON_PULL, button_event_handler}
    };

    err_code = app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);

    err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function to handle interface up event. */
void nrf51_driver_interface_up(void)
{
    APPL_LOG ("[APPL]: IPv6 interface up.\r\n");

    sys_check_timeouts();

    m_display_state = LEDS_IPV6_IF_UP;
}


/**@brief Function to handle interface down event. */
void nrf51_driver_interface_down(void)
{
    APPL_LOG ("[APPL]: IPv6 interface down.\r\n");

    m_display_state = LEDS_IPV6_IF_DOWN;
}


/**@brief Function for initializing the transport of IPv6. */
static void ipv6_transport_init(void)
{
    ble_stack_init();
    advertising_init();
}


/**@brief Function for initializing IP stack.
 *
 * @details Initialize the IP Stack and its driver.
 */
static void ip_stack_init(void)
{
    uint32_t err_code;

    //Initialize memory manager.
    err_code = nrf51_sdk_mem_init();
    APP_ERROR_CHECK(err_code);

    //Initialize lwip stack driver.
    err_code = nrf51_driver_init();
    APP_ERROR_CHECK(err_code);

    //Initialize lwip stack.
    lwip_init();
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the IoT Timer. */
static void iot_timer_init(void)
{
    uint32_t err_code;

    static const iot_timer_client_t list_of_clients[] =
    {
        {app_lwip_time_tick,    LWIP_SYS_TICK_MS},
        {blink_timeout_handler, LED_BLINK_INTERVAL_MS},
        {app_coap_time_tick,    COAP_TICK_INTERVAL_MS}
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

    uint32_t err_code = coap_init(17);
    APP_ERROR_CHECK(err_code);

    coap_endpoints_init();

    iot_timer_init();

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
