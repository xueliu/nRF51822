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
 * @brief MQTT Client Implementation over LwIP Stack port on nrf51.
 *
 * This file contains the source code for MQTT Protocol over LwIP Stack for a nrf51 device.
 * The implementation is limited to MQTT Client role only.
 */


#include "mqtt.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
/*lint -save -e607 */
#include "lwip/tcp.h"
/*lint -restore -e607 */
#include "app_trace.h"
#include <stdint.h>
#include <string.h>

//#define MQTT_TRC app_trace_log
#define MQTT_TRC(...)

/**@brief MQTT Control Packet Types. */
#define MQTT_PKT_TYPE_CONNECT      0x10
#define MQTT_PKT_TYPE_CONNACK      0x20
#define MQTT_PKT_TYPE_PUBLISH      0x30
#define MQTT_PKT_TYPE_PUBACK       0x40
#define MQTT_PKT_TYPE_PUBREC       0x50
#define MQTT_PKT_TYPE_PUBREL       0x60
#define MQTT_PKT_TYPE_PUBCOMP      0x70
#define MQTT_PKT_TYPE_SUBSCRIBE    0x82 // QoS 1 for subscribe
#define MQTT_PKT_TYPE_SUBACK       0x90
#define MQTT_PKT_TYPE_UNSUBSCRIBE  0xA2
#define MQTT_PKT_TYPE_UNSUBACK     0xB0
#define MQTT_PKT_TYPE_PINGREQ      0xC0
#define MQTT_PKT_TYPE_PINGRSP      0xD0
#define MQTT_PKT_TYPE_DISCONNECT   0xE0

/**@brief MQTT Header Masks. */
#define MQTT_HEADER_QOS_MASK       0x06

/**@brief MQTT States. */
typedef enum
{
    MQTT_STATE_IDLE,                                                 /**< Idle state, implying the client entry in the table is unused/free. */
    MQTT_STATE_TCP_CONNECTING,                                       /**< TCP Connection has been requested, awaiting result of the request. */
    MQTT_STATE_TCP_CONNECTED,                                        /**< TCP Connection successfully established. */
    MQTT_STATE_CONNECTING,                                           /**< MQTT Connect Request Sent, ACK awaited. */
    MQTT_STATE_CONNECTED,                                            /**< MQTT Connection successful. */
    MQTT_STATE_PENDING_WRITE = 0x80                                  /**< State that indicates write callback is awaited for an issued request. */
}mqtt_state_t;

/**@brief MQTT Client definition to maintain information relevant to the client. */
typedef struct
{
    const char        * device_id;                                   /**< Client Id used by the Client for its unique identification with the broker. */
    struct tcp_pcb    * pcb;                                         /**< TCP Connection Reference provided by LwIP Stack. */
    const char        * p_user_name;                                 /**< User name used for the MQTT connection. */
    mqtt_data_t       * p_password;                                  /**< Password used for the MQTT connection. */
    mqtt_evt_cb_t       evt_cb;                                      /**< Application callback registered with the module to get MQTT events. */
    uint32_t            last_activity;                               /**< Ticks based on on sys_now of LwIP to maintain last activity information. */
    ip6_addr_t          broker_addr;                                 /**< Address of MQTT broker that the client is connected to. */
    uint16_t            broker_port;                                 /**< Broker's Port number. */
    uint8_t             state;                                       /**< Client's state in the connection refer \ref mqtt_state_t for possible states . */
    uint8_t             poll_abort_counter;                          /**< Poll abort counter maintained for the TCP connection. */
}mqtt_t;

#define MAX_PACKET_SIZE_IN_WORDS (MQTT_MAX_PACKET_LENGTH/4)

static mqtt_t mqtt_client[MQTT_MAX_CLIENTS];                         /**< MQTT Client table.*/
static uint32_t m_packet[MAX_PACKET_SIZE_IN_WORDS];                     /**< Buffer for creating packets on a TCP write. */
static const uint8_t m_ping_packet[2] = {MQTT_PKT_TYPE_PINGREQ, 0x00};

/**@brief Initialize MQTT Client instance. */
static void mqtt_client_instance_init(uint32_t index)
{
    mqtt_client[index].state              = MQTT_STATE_IDLE;
    mqtt_client[index].poll_abort_counter = 0;
    mqtt_client[index].p_password         = NULL;
    mqtt_client[index].p_user_name        = NULL;
    mqtt_client[index].device_id          = NULL;
}

/**@brief Encode MQTT Remaining Length. */
static void remaining_length_encode(uint16_t remaining_length, uint8_t * p_buff, uint16_t * p_digits)
{
    uint16_t index = 0;

    do {
        p_buff[index] = remaining_length % 0x80;
        remaining_length /= 0x80;

        if(remaining_length > 0)
        {
          p_buff[index] |=  0x80;
        }

        index++;

    } while (remaining_length > 0);

    *p_digits = index;
}

/**@brief Decode MQTT Remaining Length. */
static void remaining_length_decode(uint8_t * p_buff, uint16_t * p_remaining_length, uint16_t * p_digits)
{
    uint16_t index            = 0;
    uint16_t remaining_length = 0;
    uint16_t multiplier       = 1;

    do {
        remaining_length += (p_buff[index] & 0x7F) * multiplier;
        multiplier       *= 0x80;

    } while ((p_buff[index++] & 0x80) != 0);

    *p_digits           = index;
    *p_remaining_length = remaining_length;
}


err_t tcp_write_complete_cb(void *p_arg, struct tcp_pcb *tpcb, u16_t len)
{
    mqtt_client_t   index = (mqtt_client_t)(p_arg);
    mqtt_client[index].state &= (~MQTT_STATE_PENDING_WRITE);
    
    return MQTT_SUCCESS;
}


static uint32_t transport_write(mqtt_t * p_client, uint8_t * data, uint16_t data_len)
{
    uint32_t err;
    
    if ((p_client ->state & MQTT_STATE_PENDING_WRITE) == MQTT_STATE_PENDING_WRITE)
    {
        err = MQTT_ERR_BUSY;
    }
    else
    {
        tcp_sent(p_client->pcb, tcp_write_complete_cb);
        err = tcp_write(p_client->pcb, data, data_len, TCP_WRITE_FLAG_COPY);
        if(err == ERR_OK)
        {
            p_client->last_activity = sys_now();
            p_client->state |= MQTT_STATE_PENDING_WRITE;
        }
    }

    return err;
}


/**@brief Notifies application of disconnection. In case connection is not yet established, connection event is used with failure result. */
void notify_disconnection(const mqtt_client_t * p_id, uint32_t result)
{
    mqtt_evt_t    evt;
    mqtt_t * p_client = &mqtt_client[*p_id];

    if((p_client->state & MQTT_STATE_CONNECTED) != MQTT_STATE_CONNECTED)
    {
        evt.id     = MQTT_EVT_CONNECTED;
        evt.result = MQTT_CONNECTION_FAILED;
    }
    else
    {
        evt.id     = MQTT_EVT_DISCONNECTED;
        evt.result = result;
    }

    p_client->evt_cb(p_id, (const mqtt_evt_t *)&evt);
}


/**@breif Close TCP connection and clean up client instance. */
static void tcp_close_connection(const mqtt_client_t * p_id, uint32_t result)
{
    mqtt_t * p_client = &mqtt_client[*p_id];

    tcp_arg(p_client->pcb, NULL);
    tcp_sent(p_client->pcb, NULL);
    tcp_recv(p_client->pcb, NULL);

    UNUSED_VARIABLE(tcp_close(p_client->pcb));

    notify_disconnection(p_id, result);
    mqtt_client_instance_init(*p_id);
}


/**@brief Callback registered with TCP to handle incoming data on the connection. */
err_t recv_callback(void * p_arg, struct tcp_pcb * p_pcb, struct pbuf * p_buffer, err_t err)
{
    mqtt_evt_t      evt;
    mqtt_client_t   index = (mqtt_client_t)(p_arg);
    mqtt_data_t     mqtt_data;
    mqtt_topic_t    mqtt_topic;
    uint8_t       * payload;
    uint32_t        err_code;
    uint16_t        remaining_length;
    uint16_t        rl_digits;

    MQTT_TRC("[MQTT]: >> recv_callback, result 0x%08x, buffer %p\r\n", err, p_buffer);

    if (err == ERR_OK && p_buffer != NULL)
    {
        MQTT_TRC("[MQTT]: >> Packet buffer length 0x%08x \r\n", p_buffer->tot_len);
        tcp_recved(p_pcb, p_buffer->tot_len);
        payload = (uint8_t*)(p_buffer->payload);

        switch(payload[0] & 0xF0)
        {
            case MQTT_PKT_TYPE_PINGRSP:
            {
                MQTT_TRC("[MQTT]: Received PINGRSP!\r\n");
                break;
            }
            case MQTT_PKT_TYPE_PUBLISH:
            {
                remaining_length_decode(&payload[1], &remaining_length, &rl_digits);

                // Offset consists header, remaining length and topic length
                uint8_t offset = 1 + rl_digits + 2;
                
                mqtt_topic.p_topic   =  payload + offset;
                mqtt_topic.topic_len = (payload[rl_digits + 1] << 8) | payload[rl_digits + 2];

                // Packet to send ACK for publish.
                uint8_t packet[4];
                packet[0] = MQTT_PKT_TYPE_PUBACK;
                packet[1] = 0x02;

                if (payload[0] & MQTT_HEADER_QOS_MASK)
                {
                    // QoS different from 0, Message Id present.
                    offset += 2;
                    packet[2] = payload[2];
                    packet[3] = payload[3];
                }

                mqtt_data.p_data     = &payload[offset + mqtt_topic.topic_len];
                mqtt_data.data_len   = p_buffer->tot_len - (offset + mqtt_topic.topic_len);

                MQTT_TRC("[MQTT]: Received PUBLISH! %lx %lx\r\n", mqtt_data.data_len, mqtt_topic.topic_len);
                evt.param.rx_param.p_data  = &mqtt_data;
                evt.param.rx_param.p_topic = &mqtt_topic;
                evt.id                     = MQTT_EVT_DATA_RX;
                evt.result                 = MQTT_SUCCESS;

                err_code = tcp_write(mqtt_client[index].pcb, packet, 4, 1);

                if (err_code == ERR_OK)
                {
                    mqtt_client[index].last_activity = sys_now();
                }
                else
                {
                    MQTT_TRC("[MQTT]: Failed to send PUBACK!\r\n");
                }
                mqtt_client[index].evt_cb(&index, (const mqtt_evt_t *)&evt);
                break;
            }
            case MQTT_PKT_TYPE_CONNACK:
            {
                MQTT_TRC("[MQTT]: Received CONACK, MQTT connection up!\r\n");
                mqtt_client[index].state   = MQTT_STATE_CONNECTED;
                evt.id                     = MQTT_EVT_CONNECTED;
                evt.result                 = MQTT_SUCCESS;
                mqtt_client[index].evt_cb(&index, (const mqtt_evt_t *)&evt);
                break;
            }
            case MQTT_PKT_TYPE_DISCONNECT:
            {
                MQTT_TRC("[MQTT]: Received DISCONNECT\r\n");
                tcp_close_connection(&index, MQTT_SUCCESS);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else
    {
        MQTT_TRC("[MQTT]: Error receiving data, closing connection\r\n");
        tcp_close_connection(&index, MQTT_ERR_TRANSPORT_CLOSED);
    }
    UNUSED_VARIABLE(pbuf_free(p_buffer));
    return ERR_OK;
}


/**@brief TCP Connection Callback. MQTT Connection  */
static err_t tcp_connection_callback(void * p_arg, struct tcp_pcb * p_pcb, err_t err)
{
    mqtt_client_t index = (mqtt_client_t)(p_arg);
    mqtt_t * p_client = &mqtt_client[0];

    if (err == ERR_OK)
    {
        //Register callback.
        tcp_recv(p_pcb, recv_callback);

        uint8_t * payload = (uint8_t *)m_packet;
        uint8_t    connect_flags = 0x02;
        uint16_t   usr_name_len = 0;
        uint8_t    offset = 0;
        uint8_t    did_len = strlen(p_client->device_id);
#ifdef MQTT_3_1_1
        uint8_t    remaining_length = 10 + did_len + 2;
#else // then fall back to MQTT_3_1_0
        uint8_t    remaining_length = 12 + did_len + 2;
#endif //MQTT_3_1_1

        if (NULL != p_client->p_user_name)
        {
            connect_flags |= 0x80;
            usr_name_len = strlen(p_client->p_user_name);
            remaining_length += 2 + usr_name_len;

            if (NULL != p_client->p_password)
            {
                remaining_length +=  (2 + p_client->p_password->data_len);
                connect_flags    |= 0x40;
            }
        }

        MQTT_TRC("[MQTT]: Client ID len 0x%04x, Remaining Length 0x%02x\r\n", did_len, remaining_length);

        memset(payload, 0, MQTT_MAX_PACKET_LENGTH);

        MQTT_TRC("[MQTT]: Allocated packet %p, packet size 0x%08x\r\n", payload, remaining_length + 2);

#ifdef MQTT_3_1_1
        //Variable header
        const uint8_t connect_header[] = {
                                  // Control packet.
                                  MQTT_PKT_TYPE_CONNECT,

                                  // Remaining length
                                  remaining_length,

                                  // Protocol name.
                                  0x00,0x04,'M','Q', 'T', 'T',

                                  //Protocol level
                                   0x04,

                                  //Connect flag
                                  connect_flags,

                                  //Keep alive in seconds.
                                  0x00, MQTT_KEEPALIVE};
#else
        //Variable header
        const uint8_t connect_header[] = {
                                  // Control packet.
                                  MQTT_PKT_TYPE_CONNECT,

                                  // Remaining length
                                  remaining_length,

                                  // Protocol name
                                  0x00, 0x06, 'M', 'Q', 'I', 's', 'd', 'p',

                                  //Protocol level
                                  0x03,

                                  //Connect flag
                                  connect_flags,

                                  //Keep alive in seconds.
                                  0x00, MQTT_KEEPALIVE};
#endif //MQTT_3_1_1


        MQTT_TRC("[MQTT]: Packing connect_header of size 0x%08x\r\n", sizeof(connect_header));

        //Copy fixed and variable header.
        memcpy(payload,connect_header, sizeof(connect_header));

        offset = sizeof(connect_header);

        //Pack Payload and deviceId.
        payload[offset] = 0x00;
        offset++;

        payload[offset] = did_len;
        offset++;

        MQTT_TRC("[MQTT]: Packing client id of length 0x%08x\r\n", did_len);

        memcpy(&payload[offset],p_client->device_id,did_len);
        offset += did_len;
        //Pack User id (if any)
        if (NULL != p_client->p_user_name)
        {
            MQTT_TRC("[MQTT]: Packing user name of length 0x%08x\r\n", usr_name_len);
            payload[offset] = (usr_name_len & 0xFF00) >> 8;
            offset++;
            payload[offset] = (usr_name_len & 0x00FF);
            offset++;
            memcpy(&payload[offset], p_client->p_user_name, usr_name_len);

            //Pack password (if any)
            if (NULL != p_client->p_password)
            {
                MQTT_TRC("[MQTT]: Packing password of length 0x%08lx\r\n", p_client->p_password->data_len);
                payload[offset] = (p_client->p_password->data_len & 0xFF00) >> 8;
                offset++;
                payload[offset] = (p_client->p_password->data_len & 0x00FF);
                offset++;
                memcpy(&payload[offset], p_client->p_password->p_data, p_client->p_password->data_len);
            }
        }

        MQTT_TRC("[MQTT]: Writing connect packet of length 0x%08x\r\n", remaining_length + 2);

        //Send MQTT identification message to broker.
        err = transport_write(&mqtt_client[index], payload, remaining_length + 2);

        if (err != ERR_OK)
        {
            UNUSED_VARIABLE(mqtt_abort(&index));
        }
    }

    return err;
}

uint32_t mqtt_abort(const mqtt_client_t * p_client)
{
    mqtt_client_t index =  (*p_client);
    uint8_t state = (mqtt_client[index].state & (~MQTT_STATE_PENDING_WRITE));

    if (state != MQTT_STATE_IDLE)
    {
        tcp_abort(mqtt_client[index].pcb);
        mqtt_client[index].state = MQTT_STATE_IDLE;
    }

    return MQTT_SUCCESS;
}


void mqtt_init(void)
{
    uint32_t index;

    for (index = 0; index < MQTT_MAX_CLIENTS; index++)
    {
        mqtt_client_instance_init(index);
    }
}


static void tcp_error_handler(void * p_arg, err_t err)
{
    mqtt_client_t index = (mqtt_client_t)(p_arg);
    mqtt_client[index].state = MQTT_STATE_IDLE;
}


static err_t tcp_connection_poll(void * p_arg, struct tcp_pcb * p_pcb)
{
    mqtt_client_t index = (mqtt_client_t)(p_arg);

    mqtt_client[index].poll_abort_counter++;

    return ERR_OK;
}


static uint32_t tcp_request_connection(mqtt_client_t * p_client)
{
    mqtt_client_t index = (*p_client);

    mqtt_client[index].poll_abort_counter = 0;
    mqtt_client[index].pcb = tcp_new_ip6();
    
    err_t err = tcp_connect_ip6(mqtt_client[index].pcb, &mqtt_client[index].broker_addr, mqtt_client[index].broker_port, tcp_connection_callback);
    
    if(err != ERR_OK)
    {
        UNUSED_VARIABLE(mqtt_abort(&index));
    }
    else
    {
        tcp_arg(mqtt_client[index].pcb, (void *)index);
        tcp_err(mqtt_client[index].pcb, tcp_error_handler);
        tcp_poll(mqtt_client[index].pcb, tcp_connection_poll, 10);
        tcp_accept(mqtt_client[index].pcb, tcp_connection_callback);

        mqtt_client[index].state = MQTT_STATE_TCP_CONNECTING;
    }

    return err;
}


uint32_t mqtt_connect(mqtt_client_t * p_client, const mqtt_connect_t * p_param)
{
    // Look for a free instance if available.
    uint32_t index = 0;
    uint32_t err_code = MQTT_ERR_NO_FREE_INSTANCE;
    if (p_client == NULL)
    {
        err_code = MQTT_NULL_PARAM;
    }

    for (index = 0; index < MQTT_MAX_CLIENTS; index++)
    {
        if (mqtt_client[index].state == MQTT_STATE_IDLE)
        {
            // Found a free instance, allocate for new connection.
            err_code = MQTT_SUCCESS;
            break;
        }
    }

    if (index < MQTT_MAX_CLIENTS)
    {
        mqtt_client[index].broker_addr = p_param->broker_addr;
        mqtt_client[index].broker_port = p_param->broker_port;
        mqtt_client[index].evt_cb      = p_param->evt_cb;
        mqtt_client[index].device_id   = p_param->device_id;
        mqtt_client[index].p_user_name = p_param->p_user_name;
        mqtt_client[index].p_password  = p_param->p_password;

        err_code = tcp_request_connection((mqtt_client_t *)&index);

        if(err_code != ERR_OK)
        {
            //Free instance.
            mqtt_client_instance_init(index);
        }
        else
        {
            p_client = &index;
        }
    }
    UNUSED_VARIABLE(p_client);

    return err_code;
}


uint32_t mqtt_publish(const mqtt_client_t * p_id, const mqtt_topic_t * p_topic, const mqtt_data_t * p_data)
{
    uint32_t   err_code = MQTT_ERR_NOT_CONNECTED;
    mqtt_t   * p_client = &mqtt_client[*p_id];
    uint8_t  * payload = (uint8_t *) m_packet;
    uint16_t   rl_digits;

    MQTT_TRC("[MQTT]:[CID 0x%02lx]: >> mqtt_publish Topic size 0x%08lx, Data size 0x%08lx\r\n",
            (*p_id), p_topic->topic_len, p_data->data_len);
        
    if((p_client->state & MQTT_STATE_PENDING_WRITE) == MQTT_STATE_PENDING_WRITE)
    {
        err_code = MQTT_ERR_BUSY;
    }
    else if (p_client->state == MQTT_STATE_CONNECTED)
    {
        uint8_t remaining_length = p_topic->topic_len + 2 + p_data->data_len; // No packet Identifier.

        uint8_t offset = 0;
        memset(payload, 0, MQTT_MAX_PACKET_LENGTH);

        payload[offset] = MQTT_PKT_TYPE_PUBLISH;
        offset++;
        MQTT_TRC("[MQTT]: Packing Remaining Header of size 0x%02x at offset 0x%02x\r\n", remaining_length, offset);

        remaining_length_encode(remaining_length, &payload[offset], &rl_digits);

        payload[offset] = remaining_length;
        offset += rl_digits;

        MQTT_TRC("[MQTT]: Packing Topic length of size 0x%04lx at offset 0x%02x\r\n", p_topic->topic_len, offset);

        payload[offset] = (p_topic->topic_len & 0xFF00) >> 8;
        offset++;
        payload[offset] = (p_topic->topic_len & 0x00FF);
        offset++;

        MQTT_TRC("[MQTT]: Packing Topic offset 0x%02x\r\n", offset);
        memcpy(&payload[offset], p_topic->p_topic, p_topic->topic_len);
        offset+= p_topic->topic_len;
        MQTT_TRC("[MQTT]: Packing Data offset 0x%02x\r\n", offset);
        memcpy(&payload[offset], p_data->p_data, p_data->data_len);

        MQTT_TRC("[MQTT]: tcp_write of size 0x%08X\r\n", (remaining_length + 2));

        //Publish message
        err_code = transport_write(p_client, payload, (remaining_length + 2));
    }
    else
    {
        MQTT_TRC("[MQTT]: Packet allocation failed!\r\n");
        err_code = MQTT_NO_MEM;
    }

    MQTT_TRC("[MQTT]: << mqtt_publish\r\n");

    return err_code;
}


uint32_t mqtt_disconnect(const mqtt_client_t * p_id)
{
    uint32_t err_code = MQTT_ERR_NOT_CONNECTED;
    mqtt_t * p_client = &mqtt_client[*p_id];

    if ((p_client->state & MQTT_STATE_CONNECTED) == MQTT_STATE_CONNECTED)
    {
        const uint8_t packet[] = {MQTT_PKT_TYPE_DISCONNECT, 0x00};
        UNUSED_VARIABLE(tcp_write(p_client->pcb, (void *)packet, sizeof(packet), 1));
        tcp_close_connection(p_id, MQTT_SUCCESS);
        err_code = MQTT_SUCCESS;
    }
    else if (p_client->state == MQTT_STATE_TCP_CONNECTED)
    {
        tcp_close_connection(p_id, MQTT_SUCCESS);
        err_code = MQTT_SUCCESS;
    }

    return err_code;
}


uint32_t mqtt_subscribe(const mqtt_client_t * p_id, const mqtt_topic_t * p_topic, uint16_t packet_id)
{
    uint32_t err_code = MQTT_ERR_NOT_CONNECTED;
    mqtt_t * p_client = &mqtt_client[*p_id];
    uint8_t * payload = (uint8_t *)m_packet;
    
    if((p_client->state & MQTT_STATE_PENDING_WRITE) == MQTT_STATE_PENDING_WRITE)
    {
        err_code = MQTT_ERR_BUSY;
    }
    else if (p_client->state == MQTT_STATE_CONNECTED)
    {
        memset(payload, 0, MQTT_MAX_PACKET_LENGTH);
        payload[0] = MQTT_PKT_TYPE_SUBSCRIBE;
        payload[1] = (4 + p_topic->topic_len + 1);
        payload[2] = (packet_id & 0xFF00) << 8;
        payload[3] = (packet_id & 0x00FF);
        payload[4] = (p_topic->topic_len & 0xFF00) << 8;
        payload[5] = (p_topic->topic_len & 0x00FF);

        memcpy (&payload[6], p_topic->p_topic, p_topic->topic_len);

        payload[6 + p_topic->topic_len] = 0x01; //Qos 1

        //Send message
        err_code = transport_write(p_client, payload, (6 + p_topic->topic_len + 1));
    }

    return err_code;
}


uint32_t mqtt_unsubscribe(const mqtt_client_t * p_id, uint16_t packet_id)
{
    uint32_t err_code = MQTT_ERR_NOT_CONNECTED;
    mqtt_t * p_client = &mqtt_client[*p_id];
    uint8_t * payload = (uint8_t *)m_packet;

    if((p_client->state & MQTT_STATE_PENDING_WRITE) == MQTT_STATE_PENDING_WRITE)
    {
        err_code = MQTT_ERR_BUSY;
    }
    else if (p_client->state == MQTT_STATE_CONNECTED)
    {
        memset(payload, 0, MQTT_MAX_PACKET_LENGTH);
        payload[0] = MQTT_PKT_TYPE_UNSUBSCRIBE;
        payload[1] = 0x02;
        payload[2] = (packet_id & 0xFF00) << 8;
        payload[3] = (packet_id & 0x00FF);

        err_code = transport_write(p_client, payload, 4);
    }

    return err_code;
}


uint32_t mqtt_ping(const mqtt_client_t * p_id)
{
    mqtt_t * p_client = &mqtt_client[*p_id];
    uint32_t err_code;
    
    if((p_client->state & MQTT_STATE_PENDING_WRITE) == MQTT_STATE_PENDING_WRITE)
    {
        err_code = MQTT_ERR_BUSY;
    }
    else if(p_client->state != MQTT_STATE_CONNECTED)
    {
        err_code = MQTT_ERR_NOT_CONNECTED;
    }    

    //Ping
    err_code = transport_write(p_client, (uint8_t *)m_ping_packet, 2);
    return err_code;
}


uint32_t mqtt_live(void)
{
    uint32_t current_time = sys_now();
    uint32_t index;
    for (index = 0; index < MQTT_MAX_CLIENTS; index++)
    {
        if ((current_time - mqtt_client[index].last_activity) > ((MQTT_KEEPALIVE - 2)* 1000))
        {
            if (mqtt_client[index].state == MQTT_STATE_CONNECTED)
            {
                UNUSED_VARIABLE(mqtt_ping((mqtt_client_t *)&index));
            }
        }
    }

    return MQTT_SUCCESS;
}

