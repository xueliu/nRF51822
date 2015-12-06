/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except in compliance
with this license.

Copyright (c) Dynastream Innovations Inc. 2015
All rights reserved.
*/

/**@file
 * @brief Commands used in ANT Scan and Forward
 *
 * @defgroup ant_scan_and_forward_example ANT Scan and Forward Demo
 * @{
 * @ingroup nrf_ant_scan_and_forward
 *
 */

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#define STATUS_LIGHT_OFF                ((uint8_t) 0x00)                /**< Device light is currently off */
#define STATUS_LIGHT_ON                 ((uint8_t) 0x01)                /**< Device light is currently on */

#define COMMAND_LIGHT_OFF               ((uint8_t) 0x00)                /**< Turn off device light */
#define COMMAND_LIGHT_ON                ((uint8_t) 0x01)                /**< Turn on device light */

#define MOBILE_COMMAND_PAGE             ((uint8_t) 0x10)                /**< Data page for sending control commands */
#define DEVICE_STATUS_PAGE              ((uint8_t) 0x20)                /**< Data page for sending the device status */
#define MESH_COMMAND_PAGE               ((uint8_t) 0x30)

#define ADDRESS_ALL_NODES               ((uint8_t) 0x00)                /**< Send command to all nodes  */

#define RESERVED                        ((uint8_t) 0xFF)                /**< Reserved/Invalid value  */

#endif

/**
 *@}
 **/
