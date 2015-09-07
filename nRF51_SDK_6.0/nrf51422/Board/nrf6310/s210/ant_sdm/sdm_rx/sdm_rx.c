/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
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
 
#if defined(TRACE_UART)
    #include <stdio.h>
    #include "app_uart.h"
#endif // TRACE_UART 

#include <stdint.h>
#include "boards.h"
#include "nrf_gpio.h"
#include "sdm_rx_if.h"
#include "sdm_rx.h"
#include "nrf_error.h"
#include "nordic_common.h"

#define UART_TX_BUF_SIZE 256u                    /**< UART Tx buffer size. */
#define UART_RX_BUF_SIZE 1u                      /**< UART Rx buffer size. */

#if defined(TRACE_UART)
// Page 1 data. 
static uint8_t   m_page_1_stride_count      = 0; /**< Peer device stride count. */
// Page 2 data. 
static uint8_t   m_page_2_cadence           = 0; /**< Peer device cadence. */ 
static uint8_t   m_page_2_status            = 0; /**< Peer device status. */ 
// Page 80 data. 
static uint8_t   m_page_80_hw_revision      = 0; /**< Peer device hardware revision. */
static uint16_t  m_page_80_manufacturer_id  = 0; /**< Peer device manufacturer ID. */
static uint16_t  m_page_80_model_number     = 0; /**< Peer device model number. */
// Page 81 data. 
static uint8_t   m_page_81_sw_revision      = 0; /**< Peer device software revision. */
static uint32_t  m_page_81_serial_number    = 0; /**< Peer device serial number. */
// Miscellaneous data. 
static uint32_t  m_accumulated_stride_count = 0; /**< Peer device accumulated stride count. */


/**@brief Function for decoding the content of a memory location into a uint16_t. 
 *  
 * @param[in] p_source The memory address to start reading 2 bytes from. 
 *
 * @return uint16_t    The decoded value.
 */
static __INLINE uint16_t sdm_uint16_decode(const uint8_t * p_source) 
{
    return ((((uint16_t)((uint8_t *)p_source)[0])) | 
            (((uint16_t)((uint8_t *)p_source)[1]) << 8u));
}


/**@brief Function for decoding content of a memory location into a uint32_t. 
 *  
 * @param[in] p_source The memory address to start reading 4 bytes from. 
 *
 * @return uint32_t    The decoded value.
 */
static __INLINE uint32_t sdm_uint32_decode(const uint8_t * p_source) 
{
    return ((((uint32_t)((uint8_t *)p_source)[0]) << 0)   |
            (((uint32_t)((uint8_t *)p_source)[1]) << 8u)  |
            (((uint32_t)((uint8_t *)p_source)[2]) << 16u) |
            (((uint32_t)((uint8_t *)p_source)[3]) << 24u));
}
#endif // TRACE_UART 


#if defined(TRACE_UART)
/**@brief Function for UART error handling, which is called when an error has occurred. 
 *
 * @param[in] p_event The event supplied to the handler.
 */
void uart_error_handle(app_uart_evt_t * p_event)
{
    if ((p_event->evt_type == APP_UART_FIFO_ERROR) || 
        (p_event->evt_type == APP_UART_COMMUNICATION_ERROR))
    {
        // Copying parameters to static variables because parameters are not accessible in debugger.
        static volatile app_uart_evt_t uart_event;

        uart_event.evt_type = p_event->evt_type;
        uart_event.data     = p_event->data;
        UNUSED_VARIABLE(uart_event);  
    
        for (;;)
        {
            // No implementation needed.
        }
    }
}
#endif // TRACE_UART


uint32_t sdm_rx_init(void)
{       
#if defined(TRACE_UART)
    app_uart_comm_params_t comm_params =  
    {
        RX_PIN_NUMBER, 
        TX_PIN_NUMBER, 
        RTS_PIN_NUMBER, 
        CTS_PIN_NUMBER, 
        APP_UART_FLOW_CONTROL_DISABLED, 
        false, 
        UART_BAUDRATE_BAUDRATE_Baud38400
    }; 
        
    uint32_t err_code;
    APP_UART_FIFO_INIT(&comm_params, 
                       UART_RX_BUF_SIZE, 
                       UART_TX_BUF_SIZE, 
                       uart_error_handle, 
                       APP_IRQ_PRIORITY_LOW,
                       err_code);
    APP_ERROR_CHECK(err_code);
#endif // TRACE_UART
#if defined(TRACE_GPIO)
    //  Configure pins LED_0 and LED_1 as outputs. 
    nrf_gpio_range_cfg_output(SDM_GPIO_START, SDM_GPIO_STOP);
    
    //  Reset the pins if they already are in a high state. 
    nrf_gpio_pin_clear(SDM_GPIO_0);
    nrf_gpio_pin_clear(SDM_GPIO_1);
    nrf_gpio_pin_clear(SDM_GPIO_2);
#endif // TRACE_GPIO

    return NRF_SUCCESS;         
}


uint32_t sdm_rx_data_process(uint8_t * p_page_data)
{
#if defined(TRACE_UART)
    switch (p_page_data[SDM_PAGE_INDEX_PAGE_NUM])
    {
        case SDM_PAGE_1:
            // Accumulate stride count before overwriting previous value. 

            m_accumulated_stride_count += (
                (p_page_data[SDM_PAGE_1_INDEX_STRIDE_COUNT] - m_page_1_stride_count) & UINT8_MAX);
            m_page_1_stride_count       = p_page_data[SDM_PAGE_1_INDEX_STRIDE_COUNT];
            break;
            
        case SDM_PAGE_2:
            m_page_2_cadence = p_page_data[SDM_PAGE_2_INDEX_CADENCE];
            m_page_2_status  = p_page_data[SDM_PAGE_2_INDEX_STATUS];
            break;
        
        case SDM_PAGE_80:
            m_page_80_hw_revision     = p_page_data[SDM_PAGE_80_INDEX_HW_REVISION];
            m_page_80_manufacturer_id = sdm_uint16_decode(
                &p_page_data[SDM_PAGE_80_INDEX_MANUFACTURER_ID]);
            m_page_80_model_number    = sdm_uint16_decode(
                &p_page_data[SDM_PAGE_80_INDEX_MODEL_NUMBER]);
            break;             
        
        case SDM_PAGE_81:
            m_page_81_sw_revision   = p_page_data[SDM_PAGE_81_INDEX_SW_REVISION];
            m_page_81_serial_number = sdm_uint32_decode(
                &p_page_data[SDM_PAGE_81_INDEX_SERIAL_NUMBER]);
            break;
        
        default:
            break;
    }
#endif // TRACE_UART

    return NRF_SUCCESS;         
}


uint32_t sdm_rx_log(uint8_t page)
{  
#if defined(TRACE_UART)
    static const char * p_location[4] = {"Laces", "Midsole", "Other", "Ankle"};
    static const char * p_battery[4]  = {"New", "Good", "OK", "Low"};
    static const char * p_health[4]   = {"OK", "Error", "Warning", ""};
    static const char * p_state[4]    = {"Inactive", "Active", "", ""};

    switch (page)
    {
        case SDM_PAGE_1:
            printf("page 1: stride count = %u, accumulated stride count = %u\n", 
                   m_page_1_stride_count, 
                   m_accumulated_stride_count);            
            break; 
        
        case SDM_PAGE_2:
            printf("page 2: location = %s, battery = %s, health = %s, state = %s, cadence = %u\n", 
                p_location[(m_page_2_status & SDM_STATUS_LOCATION_MASK) >> SDM_STATUS_LOCATION_POS], 
                p_battery[(m_page_2_status & SDM_STATUS_BATTERY_MASK) >> SDM_STATUS_BATTERY_POS], 
                p_health[(m_page_2_status & SDM_STATUS_HEALTH_MASK) >> SDM_STATUS_HEALTH_POS], 
                p_state[(m_page_2_status & SDM_STATUS_USE_STATE_MASK) >> SDM_STATUS_USE_STATE_POS],
                m_page_2_cadence);
            break;    
        
        case SDM_PAGE_80:
            printf("page 80: HW revision = %u, manufacturer ID = %u, model number = %u\n", 
                   m_page_80_hw_revision, 
                   m_page_80_manufacturer_id, 
                   m_page_80_model_number);
            break;
        
        case SDM_PAGE_81:
            printf("page 81: SW revision = %u, serial number = %u\n", 
                   m_page_81_sw_revision, 
                   m_page_81_serial_number); 
            break;
        
        case SDM_PAGE_EVENT_RX_FAIL: 
            printf("Event RX fail\n");
            break;
        
        default:
            break;
    }
#endif // TRACE_UART
#if defined(TRACE_GPIO)
    switch (page)
    {
        case SDM_PAGE_1:
        case SDM_PAGE_2:
            nrf_gpio_pin_toggle(SDM_GPIO_0);
            break;    
        
        case SDM_PAGE_80:
        case SDM_PAGE_81:
            nrf_gpio_pin_toggle(SDM_GPIO_1);
            break;
        
        case SDM_PAGE_EVENT_RX_FAIL:
            nrf_gpio_pin_toggle(SDM_GPIO_2);            
            break;
        
        default:
            break;
    }
#endif // TRACE_GPIO

    return NRF_SUCCESS;     
}
