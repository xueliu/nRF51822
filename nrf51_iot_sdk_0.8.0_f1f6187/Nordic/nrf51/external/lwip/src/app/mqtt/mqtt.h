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
 * @defgroup iot_sdk_lwip_mqtt MQTT Client on LwIP
 * @addtogroup iot_sdk_common
 * @{ *
 * @brief MQTT Client Implementation over LwIP Stack port on nrf51.
 *
 * @details
 * MQTT Client's Application interface is defined in this header. The implementation assumes
 * LwIP Stack is available with TCP module enabled. Also the module is implemented to work with IPv6
 * only.
 * @note By default the implementation uses MQTT version 3.1.0. However few cloud services
 * like the Xively uses the version 3.1.1. For this please define MQTT_3_1_1 and recompile the
 * example.
 */

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include "lwip/ip6_addr.h"

#define MQTT_MAX_CLIENTS       1                                               /**< Maximum number of clients that can be managed by the module. */

#define MQTT_KEEPALIVE         60                                              /**< Keep alive time for MQTT (in seconds). Sending of Ping Requests to be keep the connection alive are governed by this value. */
#define MQTT_MAX_PACKET_LENGTH TCP_MSS                                         /**< Maximum MQTT packet size that can be sent (including the fixed and variable header). */


/**@defgroup iot_sdk_mqtt_err_code MQTT Error Codes
 *@{
 */
#define MQTT_SUCCESS              0x00000000                                   /**< Indicates success result of API or Event. */
#define MQTT_ERR_NOT_CONNECTED    0x00000001                                   /**< Indicates client instance for which procedure was requested is not connected. */
#define MQTT_ERR_WRITE            0x00000002                                   /**< Indicates failure in wirte operation on TCP connection. */
#define MQTT_ERR_NO_FREE_INSTANCE 0x00000003                                   /**< Indicates new client request cannot be connected as there are no free instances available. */
#define MQTT_NO_MEM               0x00000004                                   /**< Indicates failure due to unavailability of memory. */
#define MQTT_CONNECTION_FAILED    0x00000005                                   /**< Indicates failure in estblishing connection for the client. Failure could be at TCP or MQTT level. */
#define MQTT_NULL_PARAM           0x00000006                                   /**< Null parameter supplied to an API. */
#define MQTT_ERR_BUSY             0x00000007                                   /**< Could not process a request as it was busy. */
#define MQTT_ERR_TRANSPORT_CLOSED 0x00000008                                   /**< Indicates failure in TCP connection. */
/**@}
 */

 /**@brief Reference to an MQTT Client allocated on mqtt_connect request. Freed on disconnection or failure in connection establishment. */
typedef uint32_t mqtt_client_t;

/**@brief MQTT Asynchronous Events notified to the application to the module through the callback registered by the application. */
typedef enum
{
    MQTT_EVT_CONNECTED,                                                        /**< Connection Event. Event result accompanying the event indicates whether the connection failed or succeeded. */
    MQTT_EVT_DISCONNECTED,                                                     /**< Disconnection Event. MQTT Client Reference is no longer valid once this event is received for the client. */
    MQTT_EVT_DATA_RX                                                           /**< Data Event. Notified to the application when data for a topic is received though a Publish packet from the broker. */
} mqtt_evt_identifier_t;

/**@brief Abstracts MQTT UTF-8 topic that can be subscribed to or published. */
typedef struct
{
    uint8_t    * p_topic;                                                      /**< Pointer to memory region containing the topic. */
    uint32_t     topic_len;                                                    /**< Length of topic. Note that the topic is expected to be UTF-8 format and need not be null terminated. */
} mqtt_topic_t;

/**@brief Abstracts binary data received or published for a topic. */
typedef struct
{
    uint8_t    * p_data;                                                       /**< Pointer to memory region containing the data. */
    uint32_t     data_len;                                                     /**< Length of topic. */
} mqtt_data_t;

/**@brief Event parameters when MQTT_EVT_DATA_RX event is notified to the application. */
typedef struct
{
    mqtt_topic_t * p_topic;                                                    /**< Topic on which data was published. */
    mqtt_data_t  * p_data;                                                     /**< Data published. */
} mqtt_evt_rx_param_t;

/**
 * @brief Defines event parameters notified along with asynchronous events to the application.
 *        Currently, only MQTT_EVT_DATA_RX is accompanied with parameters.
 */
typedef union
{
    mqtt_evt_rx_param_t  rx_param;                                             /**< Parameters accompanying MQTT_EVT_DATA_RX event. */
} mqtt_evt_param_t;

/**@brief Defined MQTT asynchronous event notified to the application. */
typedef struct
{
    mqtt_evt_identifier_t id;                                                  /**< Identifies the event. */
    mqtt_evt_param_t      param;                                               /**< Contains parameters (if any) accompanying the event. */
    uint32_t              result;                                              /**< Event result. For example, MQTT_EVT_CONNECTED has a result code indicating success or failure code of connection procedure. */
} mqtt_evt_t;

/**@brief Asynchronous event notification callback registered by the application with the module to receive module events. */
typedef void (*mqtt_evt_cb_t)(const mqtt_client_t * p_client, const mqtt_evt_t * p_evt);

/**@brief Connection parameters when requesting a new MQTT Client connection. */
typedef struct
{
    ip6_addr_t         broker_addr;                                           /**< IPv6 Address of MQTT broker to which client connection is requested. */
    uint16_t           broker_port;                                           /**< Broker's TCP Port number. */
    const char       * device_id;                                             /**< Unique client identification to be used for the connection. */
    const char       * p_user_name;                                           /**< User name (if any) to be used for the connection. NULL indicates no user name. */
    mqtt_data_t      * p_password;                                            /**< Password (if any) to be used for the connection. Note that if password is provided, user name shall also be provided. NULL indicates no password. */
    mqtt_evt_cb_t      evt_cb;                                                /**< Event callback registered to receive events for the client instance. */
} mqtt_connect_t;


/**
 * @brief Initializes the module. This shall be called before initiating any procedures on the module.
 */
void mqtt_init(void);


/**
 * @brief This API should be called periodically for the module to be able to keep the connection
 *        alive by sending Ping Requests if need be.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_live(void);


/**
 * @brief API to request new MQTT client connection.
 *
 * @param[out] p_client  Client instance reference allocated for the connection in case procedure succeeded.
 *                       This reference is used for requesting any procedure on the connection, hence, application shall remember it.
 * @param[in]  p_param   Parameters to be used for the MQTT client connection.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_connect(mqtt_client_t * p_client, const mqtt_connect_t * p_param);


/**
 * @brief API to request publishing data on the connection.
 *
 * @param[in]  p_client  Identifies client instance on which data is to be published.
 * @param[in]  p_topic   Topic for which data is published (shall not be NULL).
 * @param[in]  p_data    Data to be published (shall not be NULL).
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 *
 * @note Default protocol revision used for connection request is 3.1.0. Please define MQTT_3_1_1
 * to use protocol 3.1.1.
 */
uint32_t mqtt_publish(const mqtt_client_t * p_client, const mqtt_topic_t * p_topic, const mqtt_data_t * p_data);


/**
 * @brief API to request subscribe to a topic on the connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 * @param[in]  p_topic   Topic for subscription is requested (shall not be NULL).
 * @param[in]  packet_id Packet identifier to uniquely identify subscription.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_subscribe(const mqtt_client_t * p_client, const mqtt_topic_t * p_topic, uint16_t packet_id);


/**
 * @brief API to request un-subscribe from a topic on the connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 * @param[in]  packet_id Packet identifier that uniquely identify subscription being un-subscribed from.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_unsubscribe(const mqtt_client_t * p_client, uint16_t packet_id);


/**
 * @brief API to abort MQTT connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_abort(const mqtt_client_t * p_client);


/**
 * @brief API to disconnect MQTT connection.
 *
 * @param[in]  p_client  Identifies client instance for which procedure is requested.
 *
 * @retval MQTT_SUCCESS or an result code indicating reason for failure.
 */
uint32_t mqtt_disconnect(const mqtt_client_t * p_client);

#endif /* MQTT_H_ */

/**@}  */

