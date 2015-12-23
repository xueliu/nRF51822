/*
This software is subject to the license described in the license.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Dynastream Innovations Inc. 2015
All rights reserved.
*/

/**@file
 * @brief ANT Scan and Forward implementation
 *
 * @defgroup ant_scan_and_forward_example ANT Scan and Forward Demo
 * @{
 * @ingroup nrf_ant_scan_and_forward
 *
 */

#include "scan_and_forward.h"
#include "deviceregistry.h"
#include "ant_parameters.h"
#include "message_cache.h"
#include "commands.h"
#include "boards.h"
#include "app_button.h"
#include "nrf_delay.h"
#include "ant_channel_config.h"
#include "ant_search_config.h"
#include "nordic_common.h"

#define ANT_NETWORK_NUMBER          ((uint8_t) 0)       /**< Default public network number. */

#define ANT_BS_CHANNEL_TYPE         CHANNEL_TYPE_SLAVE  /**< Bi-directional slave */
#define ANT_MS_CHANNEL_TYPE         CHANNEL_TYPE_MASTER /**< Bi-directional master */

#define ANT_MESH_NETWORK_ID         ((uint8_t) 1)       /**< Network Number for Mesh Application */

#define ANT_BS_DEVICE_NUMBER        ((uint16_t) 0)      /**< Wild card the device number on the background scanning channel */

#define ANT_DEVICE_TYPE             ((uint8_t) 1)                                   /**< Device type of Scan and Forward beacon */
#define ANT_TRANSMISSION_TYPE       ((uint8_t) ((ANT_MESH_NETWORK_ID << 4) | 0x05)) /**< Transmission type is comprised of ANT_MESH_NETWORK_ID and 0x05 */

#define ANT_FREQUENCY               ((uint8_t) 25)      /**< 2425 MHz */
#define ANT_CHANNEL_PERIOD          ((uint16_t) 1024)   /**< 32Hz */
#define ANT_CHANNEL_PERIOD_NONE     ((uint16_t) 0x00)   /**< This is not taken into account. */

#define ANT_MAX_NODES_IN_NETWORK    MAX_DEVICES         /**< Max number of nodes in network. Must be <= MAX_DEVICES in device registry */

#define ANT_MAX_CACHE_SIZE          255                 /**< Max size of the received message buffer */
#define ANT_CACHE_TIMEOUT_IN_SEC    15                  /**< Max duration cached messages will be saved */

#define ANT_PAGE_PATTERN_DIVISOR    2                   /**< Helps control how often pages are interleaved */

static uint8_t m_node_address;                                  /**< Unique address of node within the network */
static uint8_t m_counter = 0;                                   /**< Index of next device */
static uint8_t m_tx_buffer[ANT_STANDARD_DATA_PAYLOAD_SIZE];     /**< Primary data transmit buffer. */
static uint8_t m_cmd_tx_buffer[ANT_STANDARD_DATA_PAYLOAD_SIZE]; /**< Command data transmit buffer. */
static uint8_t m_tx_page_counter = 0;                           /**< Transmitted page counter. */

static mesh_message_t  buffer[ANT_MAX_CACHE_SIZE]; /**< Received message cache buffer. */
static message_cache_t m_rcvd_messages =           /**< Received message cache. */
{
    0,
    0,
    0,
    ANT_MAX_CACHE_SIZE,
    buffer
};


/**@brief DEVICE_STATUS_PAGE message is queued to send.
 *
 */
static void sf_master_beacon_message_send(void);

/**@brief Checks if a message has already been received
 *
 * @param[in]   p_buffer            Pointer to buffer with message contents
 */
static bool msg_already_received(uint8_t * p_buffer);

/** @brief Sets up and opens the background scanning channel and the master beacon channel used for the scan and forward demo.
 */
static void sf_ant_channels_setup(void)
{
    uint32_t err_code;

    ant_channel_config_t ms_channel_config =
    {
        .channel_number    = SF_ANT_MS_CHANNEL_NUMBER,
        .channel_type      = ANT_MS_CHANNEL_TYPE,
        .ext_assign        = 0x00,
        .rf_freq           = ANT_FREQUENCY,
        .transmission_type = ANT_TRANSMISSION_TYPE,
        .device_type       = ANT_DEVICE_TYPE,
        .device_number     = m_node_address,
        .channel_period    = ANT_CHANNEL_PERIOD,
        .network_number    = ANT_NETWORK_NUMBER,
    };

    const ant_channel_config_t bs_channel_config =
    {
        .channel_number    = SF_ANT_BS_CHANNEL_NUMBER,
        .channel_type      = ANT_BS_CHANNEL_TYPE,
        .ext_assign        = EXT_PARAM_ALWAYS_SEARCH,
        .rf_freq           = ANT_FREQUENCY,
        .transmission_type = ANT_TRANSMISSION_TYPE,
        .device_type       = ANT_DEVICE_TYPE,
        .device_number     = ANT_BS_DEVICE_NUMBER,
        .channel_period    = ANT_CHANNEL_PERIOD_NONE,
        .network_number    = ANT_NETWORK_NUMBER,
    };

    const ant_search_config_t bs_search_config =
    {
        .channel_number        = SF_ANT_BS_CHANNEL_NUMBER,
        .low_priority_timeout  = ANT_LOW_PRIORITY_TIMEOUT_DISABLE,
        .high_priority_timeout = ANT_HIGH_PRIORITY_SEARCH_DISABLE,
        .search_sharing_cycles = ANT_SEARCH_SHARING_CYCLES_DISABLE,
        .search_priority       = ANT_SEARCH_PRIORITY_DEFAULT,
        .waveform              = ANT_WAVEFORM_FAST,
    };

    err_code = ant_channel_init(&ms_channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = ant_channel_init(&bs_channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = ant_search_init(&bs_search_config);
    APP_ERROR_CHECK(err_code);

    // Fill tx buffer for the first frame
    sf_master_beacon_message_send();

    // Open master beacon channel
    err_code = sd_ant_channel_open(SF_ANT_MS_CHANNEL_NUMBER);
    APP_ERROR_CHECK(err_code);

    // Open background scanning channel
    err_code = sd_ant_channel_open(SF_ANT_BS_CHANNEL_NUMBER);
    APP_ERROR_CHECK(err_code);
}


void sf_init(void)
{
    dr_init();

    // Get the node address from the switches instead of serial number
#if defined(NODE_ID_FROM_SWITCHES)
    m_node_address = 1 + (nrf_gpio_pin_read(BSP_SWITCH_1) << 0) |
                     (nrf_gpio_pin_read(BSP_SWITCH_2) << 1) |
                     (nrf_gpio_pin_read(BSP_SWITCH_3) << 2) |
                     (nrf_gpio_pin_read(BSP_SWITCH_4) << 3);
#else
    // Get the node address from the serial number of the device
    m_node_address = (uint8_t)(NRF_FICR->DEVICEID[0]);

    if (m_node_address == 0) // Node address 0 is not allowed
    {
        m_node_address = 1;
    }
    else if (m_node_address == 0xFF) // Node address 0xFF is not allowed
    {
        m_node_address = 0xFE;
    }
#endif

    UNUSED_PARAMETER(dr_device_add(m_node_address));

    for (int i = 0; i < ANT_STANDARD_DATA_PAYLOAD_SIZE; i++)
    {
        m_cmd_tx_buffer[i] = RESERVED;
    }

    sf_ant_channels_setup();
}


void sf_bsp_evt_handler(bsp_event_t evt)
{
    device_t * p_device;

    switch (evt)
    {
        case BSP_EVENT_KEY_0: // Turn self on
            // Update status device registry
            p_device                    = dr_device_get(m_node_address);
            p_device->application_state = STATUS_LIGHT_ON;
            p_device->last_message_sequence_received++;
            LEDS_ON(BSP_LED_0_MASK);
            break;

        case BSP_EVENT_KEY_1: // Turn self off
            // Update status device registry
            p_device                    = dr_device_get(m_node_address);
            p_device->application_state = STATUS_LIGHT_OFF;
            p_device->last_message_sequence_received++;
            LEDS_OFF(BSP_LED_0_MASK);
            break;

        case BSP_EVENT_KEY_2: // Turn all on
            // Update status device registry
            p_device                    = dr_device_get(m_node_address);
            p_device->application_state = STATUS_LIGHT_ON;
            p_device->last_message_sequence_received++;
            // Update command buffer
            set_cmd_buffer(ADDRESS_ALL_NODES, COMMAND_LIGHT_ON, m_cmd_tx_buffer[6] + 1);
            LEDS_ON(BSP_LED_0_MASK);
            break;

        case BSP_EVENT_KEY_3: // Turn all off
            // Update status device registry
            p_device                    = dr_device_get(m_node_address);
            p_device->application_state = STATUS_LIGHT_OFF;
            p_device->last_message_sequence_received++;
            // Update command buffer
            set_cmd_buffer(ADDRESS_ALL_NODES, COMMAND_LIGHT_OFF, m_cmd_tx_buffer[6] + 1);
            LEDS_OFF(BSP_LED_0_MASK);
            break;

        default:
            return; // no implementation needed
    }
}


void sf_background_scanner_process(ant_evt_t * p_ant_evt)
{
    device_t    * p_device;
    ANT_MESSAGE * p_ant_message = (ANT_MESSAGE *)p_ant_evt->evt_buffer;

    switch (p_ant_evt->event)
    {
        case EVENT_RX:

            // Device Status Page
            if (p_ant_message->ANT_MESSAGE_aucPayload[0] == DEVICE_STATUS_PAGE)
            {
                uint8_t node            = p_ant_message->ANT_MESSAGE_aucPayload[1]; // Destination
                uint8_t sequence_number = p_ant_message->ANT_MESSAGE_aucPayload[6];
                uint8_t device_state    = p_ant_message->ANT_MESSAGE_aucPayload[7];

                // Has this device been seen before?
                if (dr_device_exists(node))
                {
                    // Is this a new message?
                    if (!msg_already_received(p_ant_message->ANT_MESSAGE_aucPayload))
                    {
                        // Update status device registry
                        p_device = dr_device_get(node);
                        p_device->last_message_sequence_received = sequence_number;
                        p_device->application_state              = device_state;
                        m_tx_page_counter                        = dr_index_of_node_get(node) - 1; // Small optimization to send the new device info first
                    }
                }
                // First message received from this device.
                else
                {
                    // Attempt to add the new device
                    if (dr_device_add(node))
                    {
                        // Update status device registry
                        p_device                                 = dr_device_get(node);
                        p_device->application_state              = device_state;
                        p_device->last_message_sequence_received = sequence_number;
                    }
                }
            }
            // Mesh Command Page
            else if (p_ant_message->ANT_MESSAGE_aucPayload[0] == MESH_COMMAND_PAGE)
            {
                // Is this a new message?
                if (!msg_already_received(p_ant_message->ANT_MESSAGE_aucPayload))
                {
                    uint8_t node            = p_ant_message->ANT_MESSAGE_aucPayload[1]; // Destination
                    uint8_t sequence_number = p_ant_message->ANT_MESSAGE_aucPayload[6];
                    uint8_t node_command    = p_ant_message->ANT_MESSAGE_aucPayload[7];

                    // Update command buffer
                    set_cmd_buffer(node, node_command, sequence_number);

                    if (node == m_node_address || node == ADDRESS_ALL_NODES)
                    {
                        // Update status device registry
                        p_device = dr_device_get(m_node_address);
                        p_device->last_message_sequence_received++;

                        switch (node_command)
                        {
                            case COMMAND_LIGHT_OFF:
                                p_device->application_state = STATUS_LIGHT_OFF;
                                LEDS_OFF(BSP_LED_0_MASK);
                                break;

                            case COMMAND_LIGHT_ON:
                                p_device->application_state = STATUS_LIGHT_ON;
                                LEDS_ON(BSP_LED_0_MASK);

                                break;
                        }
                    }
                }
            }
            break;

        default:
            break;
        
    }
}


static void sf_master_beacon_message_send(void)
{
    uint32_t      err_code;
    device_t    * p_device      = NULL;
    // Send status pages based on the page pattern divisor
    if ((m_tx_page_counter % ANT_PAGE_PATTERN_DIVISOR) != 0)
    {
        // Cycle through available device numbers until we get to a registered device.
        if (m_counter < (MAX_DEVICES - 1))
        {
            m_counter++;
        }
        else
        {
            m_counter = 0;
        }

        while (!dr_device_at_index_exists(m_counter))
        {
            if (m_counter < (MAX_DEVICES - 1))
            {
                m_counter++;
            }
            else
            {
                m_counter = 0;
            }
        }

        p_device = dr_device_at_index_get(m_counter);

        // Update the primary tx buffer
        m_tx_buffer[0] = DEVICE_STATUS_PAGE;
        m_tx_buffer[1] = p_device->node_id;
        m_tx_buffer[2] = RESERVED;
        m_tx_buffer[3] = RESERVED;
        m_tx_buffer[4] = RESERVED;
        m_tx_buffer[5] = RESERVED;
        m_tx_buffer[6] = p_device->last_message_sequence_received;
        m_tx_buffer[7] = p_device->application_state;

        // Add the message we are transmitting to the cache
        mc_add(&m_rcvd_messages, m_tx_buffer);

        err_code = sd_ant_broadcast_message_tx(SF_ANT_MS_CHANNEL_NUMBER,
                                               ANT_STANDARD_DATA_PAYLOAD_SIZE,
                                               m_tx_buffer);
        APP_ERROR_CHECK(err_code);
    }
    // Send command page when we are not sending status pages
    else
    {
        // Add the message we are transmitting to the cache
        mc_add(&m_rcvd_messages, m_cmd_tx_buffer);

        err_code = sd_ant_broadcast_message_tx(SF_ANT_MS_CHANNEL_NUMBER,
                                               ANT_STANDARD_DATA_PAYLOAD_SIZE,
                                               m_cmd_tx_buffer);
        APP_ERROR_CHECK(err_code);
    }

    // Increment transmitted page counter
    m_tx_page_counter++;

    // Piggy back off of the channel period to provide 1 second ticks for cache cleanup
    if ((m_tx_page_counter % (ANT_CLOCK_FREQUENCY / ANT_CHANNEL_PERIOD)) == 0)
    {
        mc_cleanup(&m_rcvd_messages, ANT_CACHE_TIMEOUT_IN_SEC);
    }
}


void sf_master_beacon_process(ant_evt_t * p_ant_evt)
{
    device_t    * p_device      = NULL;
    ANT_MESSAGE * p_ant_message = (ANT_MESSAGE *)p_ant_evt->evt_buffer;

    switch (p_ant_evt->event)
    {
        case EVENT_TX:

            sf_master_beacon_message_send();
            break;

        case EVENT_RX:

            // Mobile Command Page
            if (p_ant_message->ANT_MESSAGE_aucPayload[0] == MOBILE_COMMAND_PAGE)
            {
                uint8_t destination_node = p_ant_message->ANT_MESSAGE_aucPayload[2];
                uint8_t command          = p_ant_message->ANT_MESSAGE_aucPayload[7];

                if (dr_device_exists(destination_node))
                {
                    p_device = dr_device_get(destination_node);
                }

                switch (command)
                {
                    case COMMAND_LIGHT_ON:

                        if (destination_node == ADDRESS_ALL_NODES)
                        {
                            set_cmd_buffer(ADDRESS_ALL_NODES,
                                           COMMAND_LIGHT_ON, m_cmd_tx_buffer[6] + 1);
                        }
                        else
                        {
                            set_cmd_buffer(destination_node, COMMAND_LIGHT_ON,
                                           m_cmd_tx_buffer[6] + 1);
                        }

                        if ((p_device != NULL) &&
                            ((p_device->node_id == m_node_address) ||
                             (destination_node == ADDRESS_ALL_NODES)))
                        {
                            p_device                    = dr_device_get(m_node_address);
                            p_device->application_state = STATUS_LIGHT_ON;
                            p_device->last_message_sequence_received++;
                            LEDS_ON(BSP_LED_0_MASK);
                        }
                        break;

                    case COMMAND_LIGHT_OFF:

                        if (destination_node == ADDRESS_ALL_NODES)
                        {
                            set_cmd_buffer(ADDRESS_ALL_NODES, COMMAND_LIGHT_OFF,
                                           m_cmd_tx_buffer[6] + 1);
                        }
                        else
                        {
                            set_cmd_buffer(destination_node, COMMAND_LIGHT_OFF,
                                           m_cmd_tx_buffer[6] + 1);
                        }

                        if ((p_device != NULL) && ((p_device->node_id == m_node_address)
                                                   || (destination_node == ADDRESS_ALL_NODES)))
                        {
                            p_device                    = dr_device_get(m_node_address);
                            p_device->application_state = STATUS_LIGHT_OFF;
                            p_device->last_message_sequence_received++;
                            LEDS_OFF(BSP_LED_0_MASK);
                        }
                        break;
                }
            }
            break;

        default:
            break;
    }
}


static bool msg_already_received(uint8_t * p_buffer)
{
    if (mc_is_in_cache(&m_rcvd_messages, p_buffer))
    {
        return true;
    }

    // New message, store for future comparisons
    mc_add(&m_rcvd_messages, p_buffer);

    return false;
}


void set_cmd_buffer(uint8_t dst, uint8_t cmd, uint8_t seq)
{
    m_cmd_tx_buffer[0] = MESH_COMMAND_PAGE;
    m_cmd_tx_buffer[1] = dst;
    m_cmd_tx_buffer[2] = RESERVED;
    m_cmd_tx_buffer[3] = RESERVED;
    m_cmd_tx_buffer[4] = RESERVED;
    m_cmd_tx_buffer[5] = RESERVED;
    m_cmd_tx_buffer[6] = seq;
    m_cmd_tx_buffer[7] = cmd;

    m_tx_page_counter += m_tx_page_counter % ANT_PAGE_PATTERN_DIVISOR; // Small optimization to send the new command first
}


/**
 *@}
 **/
