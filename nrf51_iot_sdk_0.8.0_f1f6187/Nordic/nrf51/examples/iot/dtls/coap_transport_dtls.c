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

#include "nrf_error.h"
#include "nrf_drv_rng.h"
#include "sdk_config.h"
#include "iot_common.h"
#include "iot_pbuffer.h"
#include "coap_transport.h"
#include "udp_api.h"
#include "dtls.h"
#include "dtls_config.h"
#include "app_trace.h"

#define COAP_SECURE_PORT 5684

/**
 * @brief Verify NULL parameters are not passed to API by application.
 */
#define NULL_PARAM_CHECK(PARAM)                                                                    \
        if ((PARAM) == NULL)                                                                       \
        {                                                                                          \
            return (NRF_ERROR_NULL);                                                               \
        }


/**@brief UDP port information. */
typedef struct
{
    uint32_t    socket_id;                                 /**< Socket information provided by UDP. */
    uint16_t    port_number;                               /** Associated port number. */
}udp_port_t;

volatile int dont_transmit = 0;

#define PSK_ID_MAXLEN 32
#define PSK_MAXLEN 32
#define PSK_DEFAULT_IDENTITY "Client_identity"
#define PSK_DEFAULT_KEY      "secretPSK"

#define MAX_BUFFER_SIZE      CEIL_DIV(COAP_MESSAGE_DATA_MAX_SIZE, sizeof(uint32_t))

#define COAP_TRANSPORT_TRC   app_trace_log

typedef enum
{
    DTLS_SESSION_NOT_ESTABLISHED = 1,
    DTLS_HANDSHAKE_IN_PROGRESS = 2,
    DTLS_SESSION_ESTABLISHED = 3,
    NONE
}coap_dtls_state_t;

typedef enum
{
    COAP_DTLS_UNASSIGNED,
    COAP_DTLS_CLIENT,
    COAP_DTLS_SERVER    
}coap_dtls_role_t;

static const unsigned char ecdsa_priv_key[] = {
			0x41, 0xC1, 0xCB, 0x6B, 0x51, 0x24, 0x7A, 0x14,
			0x43, 0x21, 0x43, 0x5B, 0x7A, 0x80, 0xE7, 0x14,
			0x89, 0x6A, 0x33, 0xBB, 0xAD, 0x72, 0x94, 0xCA,
			0x40, 0x14, 0x55, 0xA1, 0x94, 0xA9, 0x49, 0xFA};

static const unsigned char ecdsa_pub_key_x[] = {
			0x36, 0xDF, 0xE2, 0xC6, 0xF9, 0xF2, 0xED, 0x29,
			0xDA, 0x0A, 0x9A, 0x8F, 0x62, 0x68, 0x4E, 0x91,
			0x63, 0x75, 0xBA, 0x10, 0x30, 0x0C, 0x28, 0xC5,
			0xE4, 0x7C, 0xFB, 0xF2, 0x5F, 0xA5, 0x8F, 0x52};

static const unsigned char ecdsa_pub_key_y[] = {
			0x71, 0xA0, 0xD4, 0xFC, 0xDE, 0x1A, 0xB8, 0x78,
			0x5A, 0x3C, 0x78, 0x69, 0x35, 0xA7, 0xCF, 0xAB,
			0xE9, 0x3F, 0x98, 0x72, 0x09, 0xDA, 0xED, 0x0B,
			0x4F, 0xAB, 0xC3, 0x6F, 0xC7, 0x72, 0xF8, 0x29};

static udp_port_t m_port_table[COAP_PORT_COUNT];           /**< Table maintaining association between CoAP ports and corresponding UDP socket identifiers. */
static dtls_context_t * m_dtls_context;
static session_t m_dtls_session;
static uint32_t m_secure_port_index;


static unsigned char psk_id[PSK_ID_MAXLEN] = PSK_DEFAULT_IDENTITY;
static size_t psk_id_length = sizeof(PSK_DEFAULT_IDENTITY) - 1;
static unsigned char psk_key[PSK_MAXLEN] = PSK_DEFAULT_KEY;
static size_t psk_key_length = sizeof(PSK_DEFAULT_KEY) - 1;
static uint32_t m_buffer[MAX_BUFFER_SIZE];
static uint32_t m_buffer_len;
static uint8_t m_dtls_state;
static uint8_t m_dtls_role = COAP_DTLS_UNASSIGNED;
            
int random_vector_generate(unsigned char * p_buffer, size_t size)
{
    uint8_t available;
    uint32_t err_code;
    err_code = nrf_drv_rng_bytes_available(&available);
    if (err_code == NRF_SUCCESS)
    {    
        uint16_t length = MIN(size , available);
        err_code = nrf_drv_rng_rand(p_buffer, length);
        if (err_code == NRF_SUCCESS)
        {
            return 1;
        }
    }
    
    return 0;
}

/**@brief Callback handler to receive data on the UDP port.
 *
 * @details Callback handler to receive data on the UDP port.
 *
 * @param[in]   p_socket         Socket identifier.
 * @param[in]   p_ip_header      IPv6 header containing source and destination addresses.
 * @param[in]   p_udp_header     UDP header identifying local and remote endpoints.
 * @param[in]   process_result   Result of data reception, there could be possible errors like
 *                               invalid checksum etc.
 * @param[in]   iot_pbuffer_t    Packet buffer containing the received data packet.
 *
 * @retval NRF_SUCCESS          Indicates received data was handled successfully, else an an
 *                              error code indicating reason for failure..
 */
static uint32_t port_data_callback(const udp6_socket_t * p_socket,
                                   const ipv6_header_t * p_ip_header,
                                   const udp6_header_t * p_udp_header,
                                   uint32_t              process_result,
                                   iot_pbuffer_t       * p_rx_packet)
{
    uint32_t                index;
    uint32_t                retval = NRF_ERROR_NOT_FOUND;
    
    COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_data_callback: Src Port %d Dest Port %d \r\n",
                       p_udp_header->srcport, p_udp_header->destport);
    
    //Search for the port.
    for (index = 0; index < COAP_PORT_COUNT; index++)
    {
        if ((p_udp_header->destport == COAP_SECURE_PORT) ||
            (index == m_secure_port_index))
        {
            if (m_dtls_state == DTLS_SESSION_NOT_ESTABLISHED)
            {
                memcpy(&m_dtls_session.addr.u8, p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE);
                m_dtls_session.port = p_udp_header->srcport;
                m_dtls_session.size = sizeof(m_dtls_session.addr) + sizeof(m_dtls_session.port);
                m_secure_port_index = index;
                m_dtls_role = COAP_DTLS_SERVER;                
            }
            COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_data_callback->dtls_handle_message\r\n");
            
            dtls_handle_message(m_dtls_context, &m_dtls_session, p_rx_packet->p_payload, p_rx_packet->length);
            break;
        }                
        else if (m_port_table[index].socket_id == p_socket->socket_id)
        {
            COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_data_callback->coap_transport_read\r\n");
            //Matching port found.
            coap_remote_t           remote_endpoint;
            const coap_port_t       port       = {p_udp_header->destport};

            memcpy (remote_endpoint.addr, p_ip_header->srcaddr.u8, IPV6_ADDR_SIZE);
            remote_endpoint.port_number = p_udp_header->srcport;

            //Notify the module of received data.
            retval = coap_transport_read(&port,
                                         &remote_endpoint,
                                         process_result,
                                         p_rx_packet->p_payload,
                                         p_rx_packet->length);
            break;
        }
    }

    return retval;
}


uint32_t port_write(const uint8_t * p_remote_addr, uint16_t port, uint32_t index, const uint8_t * p_data, uint16_t datalen)
{
    uint32_t                       err_code;
    udp6_socket_t                  socket;
    ipv6_addr_t                    remote_addr;
    iot_pbuffer_t                * p_buffer;
    iot_pbuffer_alloc_param_t      buffer_param;
    
    buffer_param.type   = UDP6_PACKET_TYPE;
    buffer_param.flags  = PBUFFER_FLAG_DEFAULT;
    buffer_param.length = datalen;
    
    COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_write, datalen %d \r\n", datalen);
    
    memcpy(remote_addr.u8, p_remote_addr, IPV6_ADDR_SIZE);
    
    //Allocate buffer to send the data on port.
    err_code = iot_pbuffer_allocate(&buffer_param, &p_buffer);  

    COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_write->iot_pbuffer_allocate result 0x%08X \r\n", err_code);

    if(err_code == NRF_SUCCESS)
    {
        socket.socket_id = m_port_table[index].socket_id;
        
        //Make a copy of the data onto the buffer.
        memcpy (p_buffer->p_payload, p_data, datalen);

        //Send on UDP port.
        err_code = udp6_socket_sendto(&socket,
                                      &remote_addr,
                                      port,
                                      p_buffer);

        COAP_TRANSPORT_TRC("[CoAP-DTLS]: port_write->udp6_socket_sendto result 0x%08X \r\n", err_code);
        if(err_code != NRF_SUCCESS)
        {
            //Free the allocated buffer as send procedure has failed.
            UNUSED_VARIABLE(iot_pbuffer_free(p_buffer, true));
        }
    }
    
    return err_code;
}


int dtls_transport_write(struct dtls_context_t *ctx, session_t *session, uint8 *buf, size_t len)
{
    return port_write(session->addr.u8, session->port, m_secure_port_index, (uint8_t *) buf, len);
}


int dtls_transport_read(struct dtls_context_t *ctx, session_t *session, uint8 *buf, size_t len)
{
    coap_remote_t           remote_endpoint;
    uint32_t                retval;
    const coap_port_t       port = { 
        .port_number = m_port_table[m_secure_port_index].port_number
    };
    
    memcpy (remote_endpoint.addr, session->addr.u8, IPV6_ADDR_SIZE);
    remote_endpoint.port_number = session->port;
    
    COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_transport_read\r\n");

    //Notify the module of received data.
    retval = coap_transport_read(&port,
                                 &remote_endpoint,
                                 NRF_SUCCESS,
                                 buf,
                                 len);
    return retval;
}


int dtls_psk_calbback(struct dtls_context_t *ctx,
		              const session_t *session,
		              dtls_credentials_type_t type,
		              const unsigned char *desc, size_t desc_len,
		              unsigned char *result, size_t result_length)
{
    COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_psk_calbback\r\n");
    switch (type)
    {
        case DTLS_PSK_HINT:
        case DTLS_PSK_IDENTITY:
        {
            if (result_length < psk_id_length) 
            {
                return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
            }
            else
            {
                memcpy(result, psk_id, psk_id_length);
                COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_psk_calbback psk_id: %s psk_id_length: %d\r\n", psk_id, psk_id_length);
                return psk_id_length;
            }
        }         
       case DTLS_PSK_KEY:
       {
           if (desc_len != psk_id_length || memcmp(psk_id, desc, desc_len) != 0)
           {
                return dtls_alert_fatal_create(DTLS_ALERT_ILLEGAL_PARAMETER);
           } 
           else if (result_length < psk_key_length)
           {      
               return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
           }

           COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_psk_calbback psk: %s psk_length: %d\r\n", psk_key, psk_key_length);
           memcpy(result, psk_key, psk_key_length);
           return psk_key_length;
       }
       default:
           break;
    }
    
    

    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
}

int dtls_event_handler(struct dtls_context_t *ctx, session_t *session, 
		dtls_alert_level_t level, unsigned short code)
{
    switch(code)
    {
       case DTLS_EVENT_CONNECTED:
           COAP_TRANSPORT_TRC ("dtls_event_handler -> DTLS_EVENT_CONNECTED\r\n");
           m_dtls_state = DTLS_SESSION_ESTABLISHED;
           if ((m_dtls_role == COAP_DTLS_CLIENT) && (m_buffer_len != 0))
           {
               dtls_write(ctx,session, (uint8_t *) m_buffer, m_buffer_len);
           }
           break;
       default:
           break;
    }
    return 0;
}

int dtls_ecdsa_key_callback(struct dtls_context_t *ctx, 
		                    const session_t *session,
		                    const dtls_ecdsa_key_t **result)
{
    static const dtls_ecdsa_key_t ecdsa_key = {
    .curve = DTLS_ECDH_CURVE_SECP256R1,
    .priv_key = ecdsa_priv_key,
    .pub_key_x = ecdsa_pub_key_x,
    .pub_key_y = ecdsa_pub_key_y
  };
    
  COAP_TRANSPORT_TRC ("[DTLS]: get_ecdsa_key %p %p %p\r\n", ecdsa_priv_key, ecdsa_pub_key_x, ecdsa_pub_key_y);

  *result = &ecdsa_key;
  return 0;
}


int dtls_ecdsa_key_verify_callback(struct dtls_context_t *ctx, 
			                       const session_t *session,
			                       const unsigned char *other_pub_x,
			                       const unsigned char *other_pub_y,
			                       size_t key_size)
{
    COAP_TRANSPORT_TRC ("[DTLS]: verify_ecdsa_key %p %p %d\r\n", other_pub_x, other_pub_y, key_size);
    return 0;
}


/**@brief Creates port as requested in p_port.
 *
 * @details Creates port as requested in p_port.
 *
 * @param[in]   index    Index to the m_port_table where entry of the port created is to be made.
 * @param[in]   p_port   Port information to be created.
 *
 * @retval NRF_SUCCESS   Indicates if port was created successfully, else an an  error code
 *                       indicating reason for failure.
 */
static uint32_t port_create(uint32_t index, coap_port_t  * p_port)
{
    uint32_t        err_code;
    udp6_socket_t   socket;

    //Request new socket creation.
    err_code = udp6_socket_allocate(&socket);

    if (err_code == NRF_SUCCESS)
    {
        // Bind the socket to the local port.
        err_code = udp6_socket_bind(&socket, IPV6_ADDR_ANY, p_port->port_number);
        if(err_code == NRF_SUCCESS)
        {
            //Register data receive callback.
            err_code = udp6_socket_recv(&socket, port_data_callback);
            if(err_code == NRF_SUCCESS)
            {
                //All procedure with respect to port creation succeeded, make entry in the table.
                m_port_table[index].socket_id   = socket.socket_id;
                m_port_table[index].port_number = p_port->port_number;
                socket.p_app_data = &m_port_table[index];
                UNUSED_VARIABLE(udp6_socket_app_data_set(&socket));
            }            
        }

        if(err_code != NRF_SUCCESS)
        {
            //Not all procedures succeeded with allocated socket, hence free it.
            UNUSED_VARIABLE(udp6_socket_free(&socket));
        }
    }
    return err_code;
}


uint32_t coap_transport_init (const coap_transport_init_t   * p_param)
{
    uint32_t    err_code = NRF_ERROR_NO_MEM;
    uint32_t    index;

    NULL_PARAM_CHECK(p_param);
    NULL_PARAM_CHECK(p_param->p_port_table);
    
    nrf_drv_rng_init(NULL); 
    dtls_init();
    static dtls_handler_t cb = {
                                .write = dtls_transport_write,
                                .read  = dtls_transport_read,
                                .event = dtls_event_handler,
                            #ifdef DTLS_PSK
                                .get_psk_info = dtls_psk_calbback,
                            #endif /* DTLS_PSK */
                            #ifdef DTLS_ECC
                                .get_ecdsa_key = dtls_ecdsa_key_callback,
                                .verify_ecdsa_key = dtls_ecdsa_key_verify_callback
                            #endif /* DTLS_ECC */
                              };
    
     m_dtls_context = dtls_new_context(&m_secure_port_index);
     if (m_dtls_context)
     {
         dtls_set_handler(m_dtls_context, &cb);
     }

    for(index = 0; index < p_param->port_count; index++)
    {
        // Create end point for each of the CoAP ports.
        err_code = port_create(index, &p_param->p_port_table[index]);
        if (err_code != NRF_SUCCESS)
        {
            break;
        }
    }
    
    m_dtls_state = DTLS_SESSION_NOT_ESTABLISHED;

    return err_code;
}


uint32_t coap_transport_write(const coap_port_t    * p_port,
                              const coap_remote_t  * p_remote,
                              const uint8_t        * p_data,
                              uint16_t               datalen)
{
    uint32_t                       err_code = NRF_ERROR_NOT_FOUND;
    uint32_t                       index;
    int err;

    NULL_PARAM_CHECK(p_port);
    NULL_PARAM_CHECK(p_remote);
    NULL_PARAM_CHECK(p_data);

    //Search for the corresponding port.
    for (index = 0; index < COAP_PORT_COUNT; index ++)
    {
        if (m_port_table[index].port_number == p_port->port_number)
        {
            if ((index == m_secure_port_index) && (m_dtls_state == DTLS_SESSION_ESTABLISHED))                    
            {
                COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_write as server\r\n");
                err = dtls_write(m_dtls_context, &m_dtls_session, (uint8_t *)p_data, datalen);
                if (err < 0)
                {
                    err_code = NRF_ERROR_INTERNAL;
                }
                else
                {
                    err_code = NRF_SUCCESS;
                }
            }
            else if (p_remote->port_number == COAP_SECURE_PORT)
            {
                if (m_dtls_state == DTLS_SESSION_NOT_ESTABLISHED)
                {
                    memcpy(&m_dtls_session.addr.u8, p_remote->addr, IPV6_ADDR_SIZE);
                    m_dtls_session.port = p_remote->port_number;
                    m_dtls_session.size = sizeof(m_dtls_session.addr) + sizeof(m_dtls_session.port);
                    m_secure_port_index = index;
                    
                    COAP_TRANSPORT_TRC ("[CoAP-DTLS]: dtls_connect\r\n");                    
                    err = dtls_connect(m_dtls_context, &m_dtls_session);  
                    if (err < 0)
                    {
                        err_code = NRF_ERROR_INTERNAL;
                    }
                    else
                    {
                        //Make a copy of the buffer to be sent.
                        memcpy(m_buffer, p_data, datalen);
                        m_dtls_state = DTLS_HANDSHAKE_IN_PROGRESS;
                        m_dtls_role = COAP_DTLS_CLIENT;
                        m_buffer_len = datalen;
                        err_code = NRF_SUCCESS;
                    }
                }
                else
                {
                    err_code = NRF_ERROR_BUSY;
                }
            }
            else
            {
                COAP_TRANSPORT_TRC ("[CoAP-DTLS]: port write non-secure\r\n");
                err_code = port_write(p_remote->addr, p_remote->port_number, index, p_data, datalen);
            }
            break;
        }
    }
    
    return err_code;
}
