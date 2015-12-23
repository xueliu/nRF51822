/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Dynastream Innovations Inc. 2015
All rights reserved.
*/

 /**@file
 * @brief Example of using a 'Scan and Forward' type ANT multi node solution.
 *
 * @defgroup ant_scan_and_forward_example ANT Scan and Forward Demo
 * @{
 * @ingroup nrf_ant_scan_and_forward
 *
 */


#include <stdint.h>
#include "ant_interface.h"
#include "ant_stack_handler_types.h"
#include "bsp.h"

#define SF_ANT_BS_CHANNEL_NUMBER ((uint8_t) 0) /**< Background scanning channel */
#define SF_ANT_MS_CHANNEL_NUMBER ((uint8_t) 1) /**< Master channel */

/**@brief Processes ANT message on ANT background scanning channel
 *
 * @param[in] p_ant_event ANT message content.
 */
void sf_background_scanner_process(ant_evt_t * p_ant_evt);

/**@brief Processes ANT message on ANT master beacon channel
 *
 * @details   This function handles all events on the master beacon channel.
 *            On EVENT_TX an DEVICE_STATUS_PAGE message is queued. The format is:
 *            byte[0]   = page (0x20 = DEVICE_STATUS_PAGE)
 *            byte[1]   = Node Address (Source)
 *            byte[2]   = Reserved
 *            byte[3]   = Reserved
 *            byte[4]   = Reserved
 *            byte[5]   = Reserved
 *            byte[6]   = Sequence Number
 *            byte[7]   = Application State
 *
 *            This channel may also reseive commands sent from a mobile device such as a phone, controller, or ANTwareII in the format:
 *            byte[0]   = page (0x10 = MOBILE_COMMAND_PAGE)
 *            byte[1]   = Originating node (Set to 0xFF for mobile device)
 *            byte[2]   = Destination node (0 for all nodes)
 *            byte[3]   = Reserved (0x0F)
 *            byte[4]   = Reserved (0xFF)
 *            byte[5]   = Reserved (0xFF)
 *            byte[6]   = Reserved (0xFF)
 *            byte[7]   = Command (Turn Off: 0x00, Turn On: 0x01)
 * @param[in] p_ant_event ANT message content.
 */
void sf_master_beacon_process(ant_evt_t * p_ant_evt);

/**@brief Handles BSP events.
 *
 * @param[in] evt   BSP event.
 */
void sf_bsp_evt_handler(bsp_event_t evt);

/**@brief Initializes nodes in the scan and forward demo
 */
void sf_init(void);

/**@brief Formats command for transmission
*
* @param[in]   dst                Command destination
* @param[in]   cmd                Command
* @param[in]   seq                Command sequence number
*/
void set_cmd_buffer(uint8_t dst, uint8_t cmd, uint8_t seq);

/**
 *@}
 **/
