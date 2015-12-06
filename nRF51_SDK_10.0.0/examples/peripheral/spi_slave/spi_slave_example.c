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
 
#include "spi_slave_example.h"
#include "nrf_drv_spis.h"
#include "app_error.h"
#include "bsp.h"

#define SPIS_INSTANCE_NUMBER 1

#define TX_BUF_SIZE   16u               /**< SPI TX buffer size. */      
#define RX_BUF_SIZE   TX_BUF_SIZE       /**< SPI RX buffer size. */      
#define DEF_CHARACTER 0xAAu             /**< SPI default character. Character clocked out in case of an ignored transaction. */      
#define ORC_CHARACTER 0x55u             /**< SPI over-read character. Character clocked out after an over-read of the transmit buffer. */      

static nrf_drv_spis_t const m_spis = NRF_DRV_SPIS_INSTANCE(SPIS_INSTANCE_NUMBER); 

static uint8_t m_tx_buf[TX_BUF_SIZE];   /**< SPI TX buffer. */      
static uint8_t m_rx_buf[RX_BUF_SIZE];   /**< SPI RX buffer. */          

/**@brief Function for initializing buffers.
 *
 * @param[in] p_tx_buf  Pointer to a transmit buffer.
 * @param[in] p_rx_buf  Pointer to a receive  buffer.
 * @param[in] len       Buffers length.
 */
static __INLINE void spi_slave_buffers_init(uint8_t * const p_tx_buf, uint8_t * const p_rx_buf, const uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        p_tx_buf[i] = (uint8_t)('a' + i);
        p_rx_buf[i] = 0;
    }
}

/**@brief Function for checking if received data is valid.
 *
 * @param[in] p_rx_buf  Pointer to a receive  buffer.
 * @param[in] len       Buffers length.
 *
 * @retval true     Buffer contains expected data.
 * @retval false    Data in buffer are different than expected.
 */
static bool __INLINE spi_slave_buffer_check(uint8_t * const p_rx_buf, const uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        if (p_rx_buf[i] != (uint8_t)('A' + i))
        {
            return false;
        }
    }
    return true;
}

/**@brief Function for SPI slave event callback.
 *
 * Upon receiving an SPI transaction complete event, LED1 will blink and the buffers will be set.
 *
 * @param[in] event SPI slave driver event.
 */
static void spi_slave_event_handle(nrf_drv_spis_event_t event)
{
    uint32_t err_code;
    
    if (event.evt_type == NRF_DRV_SPIS_XFER_DONE)
    {
        err_code = bsp_indication_set(BSP_INDICATE_RCV_OK);
        APP_ERROR_CHECK(err_code);
        
        //Check if buffer size is the same as amount of received data.
        APP_ERROR_CHECK_BOOL(event.rx_amount == RX_BUF_SIZE);
        
        //Check if received data is valid.
        bool success = spi_slave_buffer_check(m_rx_buf, event.rx_amount);
        APP_ERROR_CHECK_BOOL(success);
        
        //Set buffers.
        err_code = nrf_drv_spis_buffers_set(&m_spis, m_tx_buf, sizeof(m_tx_buf), m_rx_buf, sizeof(m_rx_buf));
        APP_ERROR_CHECK(err_code);          
    }
}

/**@brief Function for initializing SPI slave.
 *
 *  Function configures a SPI slave and sets buffers.
 *
 * @retval NRF_SUCCESS  Initialization successful.
 */

uint32_t spi_slave_example_init(void)
{
    uint32_t              err_code;
    nrf_drv_spis_config_t spis_config = NRF_DRV_SPIS_DEFAULT_CONFIG(SPIS_INSTANCE_NUMBER); 

    spis_config.miso_pin        = SPIS_MISO_PIN;
    spis_config.mosi_pin        = SPIS_MOSI_PIN;
    spis_config.sck_pin         = SPIS_SCK_PIN;
    spis_config.csn_pin         = SPIS_CSN_PIN;
    spis_config.mode            = NRF_DRV_SPIS_MODE_0;
    spis_config.bit_order       = NRF_DRV_SPIS_BIT_ORDER_LSB_FIRST;
    spis_config.def             = DEF_CHARACTER;
    spis_config.orc             = ORC_CHARACTER;
    
    err_code = nrf_drv_spis_init(&m_spis, &spis_config, spi_slave_event_handle);
    APP_ERROR_CHECK(err_code);
    
    //Initialize buffers.
    spi_slave_buffers_init(m_tx_buf, m_rx_buf, (uint16_t)TX_BUF_SIZE);
    
    //Set buffers.
    err_code = nrf_drv_spis_buffers_set(&m_spis, m_tx_buf, sizeof(m_tx_buf), m_rx_buf, sizeof(m_rx_buf));
    APP_ERROR_CHECK(err_code);            

    return NRF_SUCCESS;
}
