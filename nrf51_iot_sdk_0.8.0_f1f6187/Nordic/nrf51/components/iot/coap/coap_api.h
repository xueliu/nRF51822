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

 /** @file coap_api.h
 *
 * @defgroup iot_sdk_coap_api CoAP Application Programming Interface
 * @ingroup iot_sdk_coap
 * @{
 * @brief Public API of Nordic's CoAP implementation.
 *
 */
 
#ifndef COAP_API_H__
#define COAP_API_H__

#include <stdint.h>
#include "coap_transport.h"
#include "coap_codes.h"
#include "sdk_config.h"


/**@defgroup COAP_CONTENT_TYPE_MASK Resource content type bitmask values
 * @{ */
#define COAP_CT_MASK_PLAIN_TEXT          0x01                     /**< Content type Plain text supported in the endpoint resource. */
#define COAP_CT_MASK_CHARSET_UTF8        0x02                     /**< Content type Charset-UTF8 supported in the endpoint resource. */
#define COAP_CT_MASK_APP_LINK_FORMAT     0x04                     /**< Content type Application/link-format supported in the endpoint resource. */
#define COAP_CT_MASK_APP_XML             0x08                     /**< Content type Application/xml supported in the endpoint resource. */
#define COAP_CT_MASK_APP_OCTET_STREAM    0x10                     /**< Content type Application/octet-stream supported in the endpoint resource. */
#define COAP_CT_MASK_APP_EXI             0x20                     /**< Content type Application/exi supported in the endpoint resource. */
#define COAP_CT_MASK_APP_JSON            0x40                     /**< Content type Application/json supported in the endpoint resource. */
/**@} */    

/**@defgroup COAP_METHOD_PERMISSION Resource method permission bitmask values
 * @{ */
#define COAP_PERM_NONE                   0x0000                   /**< Permission by default. Do not allow any method in the COAP/COAPS endpoint resource. */
#define COAP_PERM_GET                    0x0001                   /**< Permission to allow GET method in the COAP endpoint resource. */
#define COAP_PERM_POST                   0x0002                   /**< Permission to allow POST method in the COAP endpoint resource.  */
#define COAP_PERM_PUT                    0x0004                   /**< Permission to allow PUT method in the COAP endpoint resource.  */
#define COAP_PERM_DELETE                 0x0008                   /**< Permission to allow DELETE method in the COAP endpoint resource.  */
#define COAPS_PERM_GET                   0x0010                   /**< Permission to allow GET method in the COAPS endpoint resource.  */
#define COAPS_PERM_POST                  0x0020                   /**< Permission to allow POST method in the COAPS endpoint resource.  */
#define COAPS_PERM_PUT                   0x0040                   /**< Permission to allow PUT method in the COAPS endpoint resource.  */
#define COAPS_PERM_DELETE                0x0080                   /**< Permission to allow DELETE method in the COAPS endpoint resource.  */
#define COAP_PERM_OBSERVE                0x0100                   /**< Permission to allow OBSERVE of the endpoint resource. */
/**@} */

/**@cond */
// Forward declare structs.
typedef struct coap_message_t  coap_message_t;
typedef struct coap_resource_t coap_resource_t;
/**@endcond */

/**@brief Callback function to call upon undefined behaviour.  
 * 
 * @param[in] error_code Error code from CoAP module.
 * @param[in] p_message  CoAP message processed when error ocoured. Could be NULL. 
 */
typedef void (*coap_error_callback_t)(uint32_t error_code, coap_message_t * p_message);

/**@brief Callback function to be registered with CoAP messages.  
 *
 * @param[in]  status    Response status. Possible status codes:
 *                       NRF_SUCCESS If response was successfully received,
 *                       COAP_TRANSMISSION_RESET_BY_PEER if a reset response was recieved or,
 *                       COAP_TRANSMISSION_TIMEOUT if transmission 
 * @param[in]  p_arg     Miscellaneous pointer to application provided data that is associated with 
 *                       the message.  
 * @param[in]  p_message Pointer to a CoAP Response message. 
 */
typedef void (*coap_response_callback_t)(uint32_t status, void * p_arg, coap_message_t * p_message);

#ifdef COAP_AUTOMODE

/**@brief Callback function to be registered with CoAP endpoint resources. in auto-mode. 
 * 
 * @details The callback needs to implement any action based on the request. The p_response message 
 *          will automatically be sent as response when the callback function returns. The memory 
 *          is allocated by the caller, so the application does not have to free up the memory used
 *          for the response. 
 *
 * @param[in]  p_resource  Pointer to the request message's target resource.
 * @param[in]  p_request   Pointer to the request message.
 * @param[out] p_response  Pointer to the prepared response message. The Application can override 
 *                         its values.
 */
typedef void (*coap_method_callback_t) (coap_resource_t * p_resource, coap_message_t * p_request, coap_message_t * p_response);

#else // COAP_AUTOMODE

/**@brief Callback function to be registered with CoAP endpoint resources. in auto-mode. 
 * 
 * @details The callback needs to implement any action based on the request. The callback is 
 *          responsible of handling the sending of any response back to the requester. The memory 
 *          for p_request will be freed up by the coap module after the callback has been 
 *          completed.
 * 
 * @param[in]  p_resource  Pointer to the request message's target resource.
 * @param[in]  p_request   Pointer to the request message.
 */
typedef void (*coap_method_callback_t) (coap_resource_t * p_resource, coap_message_t * p_request);

#endif // COAP_AUTOMODE

/**@brief Enumeration of CoAP content types. */
typedef enum 
{
    COAP_CT_PLAIN_TEXT       = 0,                                 /**< Plain text content format number. Default. */ 
    COAP_CT_APP_LINK_FORMAT  = 40,                                /**< Application/link-format content format number. */
    COAP_CT_APP_XML          = 41,                                /**< Application/xml content format number. */
    COAP_CT_APP_OCTET_STREAM = 42,                                /**< Application/octet-stream content format number. */
    COAP_CT_APP_EXI          = 47,                                /**< Application/exi content format number. */
    COAP_CT_APP_JSON         = 50                                 /**< Application/json content format number. */
} coap_content_type_t;

/**@brief Enumeration of CoAP options numbers. */

#define COAP_OPT_RESERVED0         0                              /**< Reserved option number. */
#define COAP_OPT_IF_MATCH          1                              /**< If-Match option number. */
#define COAP_OPT_URI_HOST          3                              /**< URI-Host option number. */
#define COAP_OPT_ETAG              4                              /**< ETag option number. */
#define COAP_OPT_IF_NONE_MATCH     5                              /**< If-None-Match option number. */
#define COAP_OPT_URI_PORT          7                              /**< URI-Port option number. */
#define COAP_OPT_LOCATION_PATH     8                              /**< Location-Path option number. */
#define COAP_OPT_URI_PATH          11                             /**< URI-Path option number. */
#define COAP_OPT_CONTENT_FORMAT    12                             /**< Content-Format option number. */
#define COAP_OPT_MAX_AGE           14                             /**< Max-Age option number. */
#define COAP_OPT_URI_QUERY         15                             /**< URI-Query option number. */
#define COAP_OPT_ACCEPT            17                             /**< Accept option number. */
#define COAP_OPT_LOCATION_QUERY    20                             /**< Location-Query option number. */
#define COAP_OPT_PROXY_URI         35                             /**< Proxy-URI option number. */
#define COAP_OPT_PROXY_SCHEME      39                             /**< Proxy-Scheme option number. */
#define COAP_OPT_SIZE1             60                             /**< Size1 option number. */
#define COAP_OPT_RESERVED1         128                            /**< Reserved option number. */
#define COAP_OPT_RESERVED2         132                            /**< Reserved option number. */
#define COAP_OPT_RESERVED3         136                            /**< Reserved option number. */
#define COAP_OPT_RESERVED4         140                            /**< Reserved option number. */


/**@brief Enumeration of CoAP message types. */
typedef enum 
{
    COAP_TYPE_CON = 0,                                            /**< Confirmable Message type. */
    COAP_TYPE_NON,                                                /**< Non-Confirmable Message type. */
    COAP_TYPE_ACK,                                                /**< Acknowledge Message type. */
    COAP_TYPE_RST                                                 /**< Reset Message type. */
} coap_msg_type_t;

/**@brief Structure to hold a CoAP option. 
 */
typedef struct 
{
    uint16_t                 number;                              /**< Option number (including the extended delta value if any). */
    uint16_t                 length;                              /**< Option length (including the extended length value in any). */
    uint8_t *                p_data;                              /**< Pointer to the memory where the data of the option is located. */
} coap_option_t;



/**@brief Structure to hold a CoAP message configuration. 
 *
 * @details The structure is used when calling the \ref coap_message_new API function. 
 *          All data supplied will be copied to the created message. 
 */
typedef struct 
{
    coap_response_callback_t response_callback;                   /**< Callback function to be called when a response matching the token is identified. */
    uint8_t                  token[8];                            /**< Message token. token_len must be set to indicate how many of the bytes should be used in the token. */
    uint8_t                  token_len;                           /**< Token size in bytes. */
    uint16_t                 id;                                  /**< Message ID. If 0 is given, the library will replace this number with an autogenerated value. */
    coap_msg_type_t          type;                                /**< Message type: COAP_TYPE_CON, COAP_TYPE_NON, COAP_TYPE_ACK, or COAP_TYPE_RST. */
    coap_msg_code_t          code;                                /**< Message code (definitions found in coap_msg_code_t). */
    
    coap_port_t              port;                                /**< Internal. Transport layer variable to associate the message with an underlying Transport Layer socket descriptor. Will be populated by the library during creation of the message. */
} coap_message_conf_t;

/**@brief Structure to hold a CoAP message header. 
 * 
 * @details This structure holds the 4-byte mandatory CoAP header. The structure uses bitfields
 *          to save memory footprint.
 */
typedef struct 
{
    uint8_t                  version   :2;                        /**< CoAP version number. The current specification RFC7252 mandates this to be version 1. The version number can be modified in sdk_config.h. */
    uint8_t                  type      :2;                        /**< Message type: COAP_TYPE_CON, COAP_TYPE_NON, COAP_TYPE_ACK, or COAP_TYPE_RST. */
    uint8_t                  token_len :4;                        /**< Length of the message token. */
    uint8_t                  code;                                /**< Message code (definitions found in @ref coap_msg_code_t). */
    uint16_t                 id;                                  /**< Message ID in little-endian format. Convertion to Network Byte Order will be handled by the library. */
} coap_message_header_t;

/**@brief Structure to hold a CoAP message. 
 *
 * @details The CoAP message structure contains both internal and public members to 
 *          serialize and deserialize data supplied from the application to a byte buffer sent 
 *          over UDP. The message structure is used both in transmission and reception, which 
 *          makes it easy to handle in an application. Updating the message should be done 
 *          using the provided APIs, not by manually assigning new values to the members directly.
 *          Reading the members, on the other hand, is fine. 
 */
struct coap_message_t
{    
    coap_remote_t            remote;                              /**< Public. Structure containing address information and port number to the remote. */
    coap_message_header_t    header;                              /**< Public. Header structure containing the mandatory CoAP 4-byte header fields. */
    uint8_t *                p_payload;                           /**< Public. Pointer to the payload buffer in the message. */
    uint16_t                 payload_len;                         /**< Public. Size of the payload in the message. */
    uint8_t                  options_count;                       /**< Public. The number of options in the message. */
    coap_option_t            options[COAP_MAX_NUMBER_OF_OPTIONS]; /**< Public. Array options in the message. */
    void *                   p_arg;                               /**< Public. Miscellaneous pointer to application provided data that is associated with the message. */
    
    coap_response_callback_t response_callback;                   /**< Internal. Function callback set by the application to be called when a response to this message is received. Should be set by the application through a configuration parameter. */
    uint8_t                  token[8];                            /**< Internal. Array holding the variable-sized message token. Should be set by the application through a configuration parameter. */
    coap_port_t              port;                                /**< Internal. Transport layer variable to associate the message with an underlying Transport Layer socket descriptor. */
    uint16_t                 options_len;                         /**< Internal. Length of the options including the mandatory header with extension bytes and option data. Accumulated every time a new options is added. */
    uint16_t                 options_offset;                      /**< Internal. Index to where the next option or payload can be added in the message's data buffer */
    uint8_t                  options_delta;                       /**< Internal. Current option number. Used to calculate the next option delta when adding new options to the message. */
    uint8_t *                p_data;                              /**< Internal. Data buffer for adding dynamically sized options and payload. */
    uint16_t                 data_len;                            /**< Internal. Length of the provided data buffer for options and payload. */
};


/**@brief Structure to hold a CoAP endpoint resource. 
*/
struct coap_resource_t
{
    uint8_t                  child_count;                         /**< Number of children in the linked list. */
    uint16_t                 permission;                          /**< Bitmask to tell which methods are permitted on the resource. Bit values available can be seen in \ref COAP_METHOD_PERMISSION. */
    coap_resource_t *        p_sibling;                           /**< Sibling pointer to the next element in the list. */
    coap_resource_t *        p_front;                             /**< Pointer to the beginning of the linked list. */
    coap_resource_t *        p_tail;                              /**< Pointer to the last added child in the list. */
    coap_method_callback_t   callback;                            /**< Callback to the resource handler. */
    uint32_t                 ct_support_mask;                     /**< Bitmask to tell which content types are supported by the resource. Bit values available can be seen in \ref COAP_CONTENT_TYPE_MASK. */
    uint32_t                 max_age;                             /**< Max age of resource endpoint value. */
    uint32_t                 expire_time;                         /**< Number of seconds until expire. */
    char                     name[COAP_RESOURCE_MAX_NAME_LEN+1];  /**< Name of the resource. Must be zero terminated. */
};

/**@brief Initializes the CoAP module.
 *
 * @details Initializes the library module and resets internal queues and resource registrations. 
 *
 * @param[in] token_rand_seed Random number seed to be used to generate the token numbers.
 *
 * @retval NRF_SUCCESS If initialization succeeded.
 */
uint32_t coap_init(uint32_t token_rand_seed);

/**@brief Register error handler callback to the CoAP module. 
 *
 * @param[in] error_callback  Function to be called upon unknown messages and failure.
 * 
 * @retval NRF_SUCCESS If registration was successful.
 */
uint32_t coap_error_handler_register(coap_error_callback_t error_callback);

/**@brief Sends a CoAP message.
 *
 * @details Sends out a request using the underlying transport layer. Before sending, the
 *          \ref coap_message_t structure is serialized and added to an internal message queue
 *          in the library.  The handle returned can be used to abort the message from being 
 *          retransmitted at any time. 
 *
 * @param[out] p_handle  Handle to the message if CoAP CON/ACK messages has been used. Returned
 *                       by reference.
 * @param[in]  p_message Message to be sent.
 * 
 * @retval NRF_SUCCESS If the message was successfully encoded and scheduled for transmission.
 */
uint32_t coap_message_send(uint32_t * p_handle, coap_message_t * p_message); 

/**@brief Abort a CoAP message.
 * 
 * @details Aborts an ongoing transmission. If the message has not yet been sent, it will be
 *          deleted from the message queue as well as stop any ongoing re-transmission of the
 *          message.
 * 
 * @param[in] handle Handle of the message to abort.
 *
 * @retval NRF_SUCCESS         If the message was successfully aborted and removed from the 
 *                             message queue.
 * @retval NRF_ERROR_NOT_FOUND If the message with the given handle was not located in the 
 *                             message queue.
 */
uint32_t coap_message_abort(uint32_t handle);

/**@brief Creates CoAP message, initializes, and allocates the needed memory.
 *
 * @details Creates a CoAP message. This is an intermediate representation of the message, 
 *          because the message will be serialized by the library before it is transmitted. The structure 
 *          is verbose to facilitate configuring the message. Options, payload, and 
 *          remote address information can be added using API function calls. 
 *
 * @param[inout] p_request Pointer to be filled by the allocated CoAP message structures.
 * @param[in]    p_config  Configuration for the message to be created. Manual configuration 
 *                         can be performed after the message creation, except for the CLIENT port 
 *                         association.
 * 
 * @retval NRF_SUCCESS If the request was successfully allocated and initialized.
 */
uint32_t coap_message_new(coap_message_t ** p_request, coap_message_conf_t * p_config);

/**@brief Deletes the CoAP request message.
 * 
 * @details Frees up memory associated with the request message.
 * 
 * @param[in] p_message Pointer to the request message to delete.
 */
uint32_t coap_message_delete(coap_message_t * p_message); 

/**@brief Adds a payload to a CoAP message.
 *
 * @details Sets a data payload to a request or response message.
 *
 * This function must be called after all CoAP options have been added.
 *       Due to internal buffers in the library, the payload will be added after any options
 *       in the buffer. If an option is added after the payload, this option will over-write 
 *       the payload in the internal buffer.
 *
 * @param[inout] p_message Pointer to the message to add the payload to.
 * @param[in] p_payload Pointer to the payload to be added.
 * @param[in] payload_len Size of the payload to be added.
 * 
 * @retval NRF_SUCCESS         If the payload was successfully added to the message.
 */
uint32_t coap_message_payload_set(coap_message_t * p_message, 
                                  void *           p_payload, 
                                  uint16_t         payload_len);

/**@brief Adds an empty CoAP option to the message.
 * 
 * Option numbers must be in ascending order, adding the one with the smallest number
 *       first and greatest last. If the order is incorrect, the delta number calculation will
 *       result in an invalid or wrong delta number for the option. 
 *
 * @param[inout] p_message  Pointer to the message to add the option to. Should not be NULL.
 * @param[in]    option_num The option number to add to the message.
 *                                  
 * @retval NRF_SUCCESS         If the empty option was successfully added to the message.
 * @retval NRF_ERROR_DATA_SIZE If the data exceeds the available message buffer data size. 
 * @retval NRF_ERROR_NO_MEM    If the maximum number of options that can be added to a message has been reached.
 */                                  
uint32_t coap_message_opt_empty_add(coap_message_t * p_message, uint16_t option_num);

/**@brief Adds a uint CoAP option to the message.
 * 
 * Option numbers must be in ascending order, adding the one with the smallest number
 *       first and greatest last. If the order is incorrect, the delta number calculation will
 *       result in an invalid or wrong delta number for the option.  
 *
 * @param[inout] p_message  Pointer to the message to add the option to. Should not be NULL.
 * @param[in]    option_num The option number to add to the message.
 * @param[in]    data       An unsigned value (8-bit, 16-bit, or 32-bit) casted to uint32_t. 
 *                          The value of the data is used to determine how many bytes
 *                          CoAP must use to represent this option value.
 *
 * @retval NRF_SUCCESS         If the unsigned integer option was successfully added to the message.
 * @retval NRF_ERROR_DATA_SIZE If the data exceeds the available message buffer data size. 
 * @retval NRF_ERROR_NO_MEM    If the maximum number of options that can be added to a message has been reached.
 */
uint32_t coap_message_opt_uint_add(coap_message_t * p_message, uint16_t option_num, uint32_t data);

/**@brief Adds a string CoAP option to the message.
 * 
 * Option numbers must be in ascending order, adding the one with the smallest number
 *       first and greatest last. If the order is incorrect, the delta number calculation will
 *       result in an invalid or wrong delta number for the option.  
 *
 * @param[inout] p_message  Pointer to the message to add the option to. Should not be NULL.
 * @param[in]    option_num The option number to add to the message.
 * @param[in]    p_data     Pointer to a string buffer to be used as value for the option. 
 *                          Should not be NULL.
 * @param[in]    length     Length of the string buffer provided.
 * 
 * @retval NRF_SUCCESS         If the string option was successfully added to the message.
 * @retval NRF_ERROR_DATA_SIZE If the data exceeds the available message buffer data size. 
 * @retval NRF_ERROR_NO_MEM    If the maximum number of options that can be added to a message has been reached.
 */
uint32_t coap_message_opt_str_add(coap_message_t * p_message, uint16_t option_num, uint8_t * p_data, uint16_t length);

/**@brief Adds an opaque CoAP option to the message.
 * 
 * Option numbers must be in ascending order, adding the one with the smallest number
 *       first and greatest last. If the order is incorrect, the delta number calculation will
 *       result in an invalid or wrong delta number for the option.  
 *
 * @param[inout] p_message  Pointer to the message to add the option to. Should not be NULL.
 * @param[in]    option_num The option number to add to the message. 
 * @param[in]    p_data     Pointer to an opaque buffer to be used as value for the option.
 *                          Should not be NULL.
 * @param[in]    length     Length of the opaque buffer provided.
 *
 * @retval NRF_SUCCESS         If the opaque option was successfully added to the message.
 * @retval NRF_ERROR_DATA_SIZE If the data exceeds the available message buffer data size. 
 * @retval NRF_ERROR_NO_MEM    If the maximum number of options that can be added to a message has been reached.
 */
uint32_t coap_message_opt_opaque_add(coap_message_t * p_message, uint16_t option_num, uint8_t * p_data, uint16_t length);

/**@brief Sets a remote address and port number to a CoAP message.
 *
 * @details Copies the content of the provided pointer into the CoAP message.
 *
 * @param[inout] p_message  Pointer to the message to add the address information to.
 *                          Should not be NULL.
 * @param[in]    p_address  Pointer to a structure holding the address information for the remote server or client.
 *                          Should not be NULL.
 *
 * @retval  NRF_SUCCESS     When copying the content has finished.
 */
uint32_t coap_message_remote_addr_set(coap_message_t * p_message, coap_remote_t * p_address);

/**@brief Creates a CoAP endpoint resource.
 * 
 * @details Initializes the \ref coap_resource_t members.
 * 
 * The first resource that is created will be set as the root of the resource
 *       hierarchy. 
 * 
 * @param[in] p_resource       Pointer to coap_resource_t passed in by reference. 
 *                             This variable must be stored in non-volatile memory.
 *                             Should not be NULL.
 * @param[in] name             Verbose name of the service (zero-terminated 
 *                             string). The maximum length of a name is defined
 *                             by COAP_RESOURCE_MAX_NAME_LEN in @c sdk_config.h
 *                             and can be adjusted if needed. Should not be NULL.
 * @retval NRF_ERROR_DATA_SIZE If the provided name is larger than the available name buffer.
 * @retval NRF_ERROR_NULL      If the pointer to the resource or the provided 
 *                             name buffer is NULL.
 */
uint32_t coap_resource_create(coap_resource_t * p_resource, const char * name);

/**@brief Adds a child resource.
 * 
 * @details The hierarchy is constructed as a linked list with a maximum number of children.
 *          COAP_RESOURCE_MAX_DEPTH in @c sdk_config.h sets the maximum depth. The maximum
 *          number of children can be adjusted if more levels are needed.
 *                                    
 * @param[in] p_parent     Resource to attach the child to. Should not be NULL.
 * @param[in] p_child      Child resource to attach. Should not be NULL.
 * 
 * @retval NRF_SUCCESS                  If the child was successfully added.
 * @retval COAP_ERROR_MAX_DEPTH_REACHED If the child is exceeding the maximum depth defined.
 */ 
uint32_t coap_resource_child_add(coap_resource_t * p_parent, coap_resource_t * p_child);

/**@brief Generates .well-known/core string.
 * 
 * @details This is a helper function for generating a CoRE link-format encoded string used for 
 *          CoAP discovery. The function traverse the resource hierarchy recursively. 
 *          The result will be resources listed in link-format. This function can be called when 
 *          all resources have been added by the application. 
 *
 * @param[inout] string Buffer to use for the .well-known/core string. Should not be NULL.
 * @param[inout] length Length of the string buffer. Returns the used number of bytes from
 *                      the provided buffer.
 * 
 * @retval NRF_SUCCESS             If string generation was successful.
 * @retval NRF_ERROR_NULL          If the string buffer was a NULL pointer.
 * @retval NRF_ERROR_DATA_SIZE     If the size of the generated string exceeds the given buffer size.
 * @retval NRF_ERROR_INVALID_STATE If no resource has been registered.
 */
uint32_t coap_resource_well_known_generate(uint8_t * string, uint16_t * length);

/**@brief Get the root resource pointer.
 * 
 * @param[out] pp_resource Pointer to be filled with pointer to the root resource.
 * 
 * @retval NRF_SUCCESS         If root resource was located.
 * @retval NRF_ERROR_NOT_FOUND If root resource was not located.
 * @retval NRF_ERROR_NULL      If output pointer was NULL.
 */
uint32_t coap_resource_root_get(coap_resource_t ** pp_resource);

/**@brief Check whether a message contains a given CoAP Option.
 *
 * @param[in]  p_message Pointer to the to check for the CoAP Option.
 *                       Should not be NULL.
 * @param[in]  option    CoAP Option to check for in the CoAP message.
 * 
 * @retval NRF_SUCCESS         If the CoAP Option is present in the message.
 * @retval NRF_ERROR_NULL      If the pointer to the message is NULL.
 * @retval NRF_ERROR_NOT_FOUND If the CoAP Option is not present in the message.
 */
uint32_t coap_message_opt_present(coap_message_t * p_message, uint16_t option);

/**@brief Find common content type between the CoAP message and the resource.
 *
 * @details The return value will be the first match between the ACCEPT options and the supported 
 *          content types in the resource. The priority is by content-format ID starting going 
 *          from the lowest value to the highest.
 * 
 * @param[out] p_ct       Resolved content type given by reference. Should not be NULL.
 * @param[in]  p_message  Pointer to the message. Should not be NULL.
 * @param[in]  p_resource Pointer to the resource. Should not be NULL.
 *
 * @retval NRF_SUCCESS         If match was found.
 * @retval NRF_ERROR_NOT_FOUND If no match was found.
 */
uint32_t coap_message_ct_match_select(coap_content_type_t * p_ct, coap_message_t * p_message, coap_resource_t * p_resource);

/**@brief CoAP time tick used for retransmitting any message in the queue if needed.
 * 
 * @retval NRF_SUCCESS         If time tick update was successfully handled.
 */
uint32_t coap_time_tick(void);

#endif // COAP_API_H__

/** @} */
