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

/** @file coap_codes.h
 *
 * @defgroup iot_sdk_coap_codes CoAP Codes
 * @ingroup iot_sdk_coap
 * @{
 * @brief CoAP message and response codes.
 */

#ifndef COAP_CODES_H__
#define COAP_CODES_H__

#define COAP_CODE(c, dd) \
    (((c & 0x7) << 5) | (dd & 0x1F))

/** @brief CoAP Message codes
*/
typedef enum
{
    // CoAP Empty Message
    COAP_CODE_EMPTY_MESSAGE                  = COAP_CODE(0,00),  //      0    0x00

    // CoAP Method Codes
    COAP_CODE_GET                            = COAP_CODE(0,01),  //      1    0x01
    COAP_CODE_POST                           = COAP_CODE(0,02),  //      2    0x02
    COAP_CODE_PUT                            = COAP_CODE(0,03),  //      3    0x03
    COAP_CODE_DELETE                         = COAP_CODE(0,04),  //      4    0x04

    // CoAP Success Response Codes
    COAP_CODE_201_CREATED                    = COAP_CODE(2,01),  //     65    0x41
    COAP_CODE_202_DELETED                    = COAP_CODE(2,02),  //     66    0x42
    COAP_CODE_203_VALID                      = COAP_CODE(2,03),  //     67    0x43
    COAP_CODE_204_CHANGED                    = COAP_CODE(2,04),  //     68    0x44
    COAP_CODE_205_CONTENT                    = COAP_CODE(2,05),  //     69    0x45

    // CoAP Client Error Response Codes
    COAP_CODE_400_BAD_REQUEST                = COAP_CODE(4,00),  //    128    0x80
    COAP_CODE_401_UNAUTHORIZED               = COAP_CODE(4,01),  //    129    0x81
    COAP_CODE_402_BAD_OPTION                 = COAP_CODE(4,02),  //    130    0x82
    COAP_CODE_403_FORBIDDEN                  = COAP_CODE(4,03),  //    131    0x83
    COAP_CODE_404_NOT_FOUND                  = COAP_CODE(4,04),  //    132    0x84
    COAP_CODE_405_METHOD_NOT_ALLOWED         = COAP_CODE(4,05),  //    133    0x85
    COAP_CODE_406_NOT_ACCEPTABLE             = COAP_CODE(4,06),  //    134    0x86
    COAP_CODE_412_PRECONDITION_FAILED        = COAP_CODE(4,12),  //    140    0x8C
    COAP_CODE_413_REQUEST_ENTITY_TOO_LARGE   = COAP_CODE(4,13),  //    141    0x8D
    COAP_CODE_415_UNSUPPORTED_CONTENT_FORMAT = COAP_CODE(4,15),  //    143    0x8F

    // CoAP Server Error Response Codes
    COAP_CODE_500_INTERNAL_SERVER_ERROR      = COAP_CODE(5,00),  //    160    0xA0
    COAP_CODE_501_NOT_IMPLEMENTED            = COAP_CODE(5,01),  //    161    0xA1
    COAP_CODE_502_BAD_GATEWAY                = COAP_CODE(5,02),  //    162    0xA2
    COAP_CODE_503_SERVICE_UNAVAILABLE        = COAP_CODE(5,03),  //    163    0xA3
    COAP_CODE_504_GATEWAY_TIMEOUT            = COAP_CODE(5,04),  //    164    0xA4
    COAP_CODE_505_PROXYING_NOT_SUPPORTED     = COAP_CODE(5,05)   //    165    0xA5
} coap_msg_code_t;

#endif // COAP_CODES_H__

/** @} */
