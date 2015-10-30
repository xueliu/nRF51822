/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include <string.h>

#include "tinydtls.h"

#include "dtls.h"
#include "debug.h"
#include "iot_common.h"

#ifdef DTLS_PSK
/* The PSK information for DTLS */
/* make sure that default identity and key fit into buffer, i.e.
 * sizeof(PSK_DEFAULT_IDENTITY) - 1 <= PSK_ID_MAXLEN and
 * sizeof(PSK_DEFAULT_KEY) - 1 <= PSK_MAXLEN
*/

#define PSK_ID_MAXLEN 32
#define PSK_MAXLEN 32
#define PSK_DEFAULT_IDENTITY "Client_identity"
#define PSK_DEFAULT_KEY      "secretPSK"
#endif /* DTLS_PSK */
static int tx_count = 0;
static int rx_count = 0;

#define MAX_PAYLOAD_LEN 60

static dtls_context_t *dtls_context;
static char buf[200];
static size_t buflen = 0;
static session_t dst;

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

static const ipv6_addr_t app_addr;

static void
try_send(struct dtls_context_t *ctx, session_t *p_dst) {
  int res;
  res = dtls_write(ctx, p_dst, (uint8 *)buf, buflen);
  if (res >= 0) {
    memmove(buf, buf + res, buflen - res);
    buflen -= res;
  }
}

static int
read_from_peer(struct dtls_context_t *ctx, 
	       session_t *session, uint8 *data, size_t len) {
  size_t i;
  for (i = 0; i < len; i++)
    PRINTF("%c", data[i]);
  return 0;
}

static uint8_t packet1[60] =
{
    //1   2     3     4     5     6     7      8    9     10
    0x16, 0xFE, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x2F, 0x03, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x23, 0xFE, 0xFD, 0x20, 0x64, 0x37,
    0xCE, 0xC1, 0x43, 0xD7, 0x42, 0xAD, 0x7E, 0xBE, 0x6F, 0xBB,
    0xE1, 0x66, 0xAF, 0x5D, 0x7A, 0x4E, 0xDD, 0x8D, 0xD5, 0xC8,
    0x62, 0xBF, 0xB4, 0x58, 0x6A, 0xC4, 0xE5, 0x38, 0x93, 0x5F
};

static uint32_t packet2[]=
{
0x00FDFE16, 0x00000000, 0x00010000, 0x00000264,
0x00010058, 0x00000000, 0x55FDFE58, 0x63C43846,
0xB8F91BCA, 0x4CFB35AB, 0x4CC89BE4, 0x03ADA419,
0xCAF96A40, 0x56E1B50E, 0x206F2282, 0xC4384655,
0x155A6069, 0x890431C4, 0xCCAFE52F, 0x28EFF40D,
0xFA6A6299, 0x0F2FB658, 0x3953729D, 0x0000AEC0,
0x00130010, 0x14000201, 0x00020100, 0x0102000B,
0xFDFE1600, 0x00000000, 0x02000000, 0x000B6D00,
0x02006100, 0x00000000, 0x00006100, 0x5B00005E,
0x13305930, 0x862A0706, 0x023DCE48, 0x2A080601,
0x3DCE4886, 0x03070103, 0xFC040042, 0xC12887C2,
0xBE55B123, 0xC0C10F41, 0x74A31D65, 0x7FBE6EFC,
0x906E6096, 0x88D127D9, 0xD2734A89, 0x9573AAFF,
0x4698767D, 0xC51CFC33, 0x3C760B4D, 0x9D9A55A0,
0xE90697FF, 0xAC7D55F4, 0x162AF5C3, 0x0000FDFE,
0x00000000, 0x9C000300, 0x9000000C, 0x00000300,
0x90000000, 0x41170003, 0x0C948504, 0x4692B317,
0x85C461C9, 0xF5479067, 0x923380F1, 0xF2FEF293,
0x7C2C2D47, 0x127885BE, 0x46E773D4, 0xDE589A0E,
0xC860D570, 0x35D9C4F0, 0x513CA5A6, 0x2EF29186,
0x3F6B16D0, 0xB0B23908, 0x000304D0, 0x02453047,
0x78432520, 0xF8400CDE, 0xD6FB490F, 0xB816C5A8,
0xD62A8205, 0x8138E7E6, 0x4CB5E772, 0x25D54FA0,
0x00210279, 0x4BB42FE7, 0x3AFC43EE, 0x86E7318A,
0x19C57E33, 0x0B9082A1, 0x24306F3C, 0x89A4B1B0,
0x30DCB28C, 0x00FDFE16, 0x00000000, 0x00040000,
0x00000D7B, 0x0004006F, 0x00000000, 0x0040016F,
0x00030402, 0x30650067, 0x300B3163, 0x55030609,
0x02130604, 0x13314843, 0x03061130, 0x0C080455,
0x6D6F530A, 0x74532D65, 0x31657461, 0x060D300F,
0x07045503, 0x755A060C, 0x68636972, 0x1A301C31,
0x04550306, 0x45130C0A, 0x5A204854, 0x63697275,
0x78452068, 0x6C706D61, 0x10317365, 0x03060E30,
0x0C030455, 0x20664307, 0x746F6F72, 0x00FDFE16,
0x00000000, 0x00050000, 0x00000E0C, 0x00050000,
0x00000000, 0x00000000
};

static uint8_t packet_to_test_server1[113] =
{
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5a, 0x01, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x4e, 0xfe, 0xfd, 0x55, 0x52, 0x21,
    0x42, 0x72, 0x39, 0xf6, 0x7a, 0x39, 0x94, 0xcd, 0x12, 0x59, 
    0x91, 0x88, 0x4e, 0x8a, 0x92, 0xda, 0xce, 0x7e, 0xe3, 0x90,
    0xd8, 0xe6, 0x57, 0x57, 0xfb, 0xbf, 0xc0, 0x5d, 0x0c, 0x00,
    0x00, 0x00, 0x04, 0xc0, 0xae, 0xc0, 0xa8, 0x01, 0x00, 0x00,
    0x20, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x17, 0x00,
    0x18, 0x00, 0x19, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 
    0x13, 0x00, 0x03, 0x02, 0x02, 0x00, 0x00, 0x14, 0x00, 0x03,
    0x02, 0x02, 0x00
};

static uint8_t packet_to_test_server2[129]=
{
    0x16, 0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x6a, 0x01, 0x00, 0x00, 0x5e, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5e, 0xfe, 0xfd, 0x55, 0x52, 0x21,
    0x42, 0x72, 0x39, 0xf6, 0x7a, 0x39, 0x94, 0xcd, 0x12, 0x59,
    0x91, 0x88, 0x4e, 0x8a, 0x92, 0xda, 0xce, 0x7e, 0xe3, 0x90,
    0xd8, 0xe6, 0x57, 0x57, 0xfb, 0xbf, 0xc0, 0x5d, 0x0c, 0x00, 
    0x10, 0x6c, 0x6a, 0x53, 0x53, 0x65, 0xc4, 0xe0, 0x3f, 0x4e,
    0x81, 0x4a, 0x4d, 0xd3, 0x76, 0x03, 0xbd, 0x00, 0x04, 0xc0, 
    0xae, 0xc0, 0xa8, 0x01, 0x00, 0x00, 0x20, 0x00, 0x0a, 0x00,
    0x08, 0x00, 0x06, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x00, 
    0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 0x13, 0x00, 0x03, 0x02,
    0x02, 0x00, 0x00, 0x14, 0x00, 0x03, 0x02, 0x02, 0x00
};

typedef struct
{
    uint32_t  len;
    uint8_t * pkt;
}packet_t;

packet_t rx_packet_table[] =
{
    {
       .len = 113,
       .pkt = packet1
    },
    {
        .len = 565,
        .pkt = (uint8_t *)packet2
    }
};

packet_t rx_packet_table_to_test_server[] =
{
    {
       .len = 113,
       .pkt = packet_to_test_server1
    },
    {
       .len = 127,
       .pkt = packet_to_test_server2
    }
};

#ifdef TEST_CLIENT
packet_t * packet_table = rx_packet_table;
#else
packet_t * packet_table = rx_packet_table_to_test_server;
#endif

void receive_from_peer(void)
{
#ifdef TEST_CLIENT
    if ((rx_count == tx_count) || (rx_count == 2))
    {
        return;
    }
    else
#endif //TEST_CLIENT
    {                 
        //create response here.
        dtls_handle_message(dtls_context, &dst, packet_table[rx_count].pkt,packet_table[rx_count].len);
        rx_count++;
    }
}
static int
send_to_peer(struct dtls_context_t *ctx, 
	     session_t *session, uint8 *data, size_t len)
{

  tx_count ++;
  if (tx_count == 1)
  {
      //Copy cookie.
      memcpy(&packet_to_test_server2[61], &data[len-16], 16);
  }
  
  return 0;  
}

#ifdef DTLS_PSK
static unsigned char psk_id[PSK_ID_MAXLEN] = PSK_DEFAULT_IDENTITY;
static size_t psk_id_length = sizeof(PSK_DEFAULT_IDENTITY) - 1;
static unsigned char psk_key[PSK_MAXLEN] = PSK_DEFAULT_KEY;
static size_t psk_key_length = sizeof(PSK_DEFAULT_KEY) - 1;

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__((unused))
#else
#define UNUSED_PARAM
#endif /* __GNUC__ */

/* This function is the "key store" for tinyDTLS. It is called to
 * retrieve a key for the given identity within this particular
 * session. */
static int
get_psk_info(struct dtls_context_t *ctx UNUSED_PARAM,
	    const session_t *session UNUSED_PARAM,
	    dtls_credentials_type_t type,
	    const unsigned char *id, size_t id_len,
	    unsigned char *result, size_t result_length)
{

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
                return psk_id_length;
            }
        }         
       case DTLS_PSK_KEY:
       {
           if (id_len != psk_id_length || memcmp(psk_id, id, id_len) != 0)
           {
                return dtls_alert_fatal_create(DTLS_ALERT_ILLEGAL_PARAMETER);
           } 
           else if (result_length < psk_key_length)
           {      
               return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
           }

           memcpy(result, psk_key, psk_key_length);
           return psk_key_length;
       }
       default:
           break;
    }

    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
}
#endif /* DTLS_PSK */

#ifdef DTLS_ECC
static int
get_ecdsa_key(struct dtls_context_t *ctx,
	      const session_t *session,
	      const dtls_ecdsa_key_t **result) {
  static const dtls_ecdsa_key_t ecdsa_key = {
    .curve = DTLS_ECDH_CURVE_SECP256R1,
    .priv_key = ecdsa_priv_key,
    .pub_key_x = ecdsa_pub_key_x,
    .pub_key_y = ecdsa_pub_key_y
  };

  *result = &ecdsa_key;
  return 0;
}

static int
verify_ecdsa_key(struct dtls_context_t *ctx,
		 const session_t *session,
		 const unsigned char *other_pub_x,
		 const unsigned char *other_pub_y,
		 size_t key_size) {
  return 0;
}
#endif /* DTLS_ECC */


/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/



void
init_dtls(session_t *p_dst) {
  static dtls_handler_t cb = {
    .write = send_to_peer,
    .read  = read_from_peer,
    .event = NULL,
#ifdef DTLS_PSK
    .get_psk_info = get_psk_info,
#endif /* DTLS_PSK */
#ifdef DTLS_ECC
    .get_ecdsa_key = get_ecdsa_key,
    .verify_ecdsa_key = verify_ecdsa_key
#endif /* DTLS_ECC */
  };

  p_dst->size = sizeof(p_dst->addr) + sizeof(p_dst->port);
  p_dst->port = HTONS(20220);

  dtls_set_log_level(DTLS_LOG_DEBUG);

  dtls_context = dtls_new_context(NULL);
  if (dtls_context)
    dtls_set_handler(dtls_context, &cb);
}

/*---------------------------------------------------------------------------*/
int main (void)
{
  static int connected = 0;  

  nrf51_sdk_mem_init();
  dtls_init();

  init_dtls(&dst);

  while(1) {
//    if(ev == tcpip_event) {
//      dtls_handle_read(dtls_context);
//    } else if (ev == serial_line_event_message) {
//      register size_t len = min(strlen(data), sizeof(buf) - buflen);
//      memcpy(buf + buflen, data, len);
//      buflen += len;
//      if (buflen < sizeof(buf) - 1)
//	buf[buflen++] = '\n';	/* serial event does not contain LF */
      #ifdef TEST_CLIENT
        if (!connected)
        {
            connected = dtls_connect(dtls_context, &dst) >= 0;
            try_send(dtls_context, &dst);            
        }
       #endif // TEST_CLIENT
        receive_from_peer();
    }
} 

int random_vector_generate(unsigned char * p_buffer, size_t size)
{
    memset(p_buffer, 0x1A, size);
    return 1;
}
/*---------------------------------------------------------------------------*/
