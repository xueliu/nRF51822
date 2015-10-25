/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 */

#include "ble_cgm.h"
#include <string.h>
#include "ble_srv_common.h"
#include "ble_racp.h"
#include "ble_cgm_db.h"
#include "ble_date_time.h"

#define OPERAND_FILTER_TYPE_RESV        0x00 /**< Filter type value reserved for future use. */
#define OPERAND_FILTER_TYPE_SEQ_NUM     0x01 /**< Filter data using Sequence Number criteria. */
#define OPERAND_FILTER_TYPE_FACING_TIME 0x02 /**< Filter data using User Facing Time criteria. */

#define SIZE_START_TIME_SESSION 7

/**@brief Glucose Service communication state. */
typedef enum
{
    STATE_NO_COMM,                 /**< The service is not in a communicating state. */
    STATE_RACP_PROC_ACTIVE,        /**< Processing requested data. */
    STATE_RACP_RESPONSE_PENDING,   /**< There is a RACP indication waiting to be sent. */
    STATE_RACP_RESPONSE_IND_VERIF, /**< Waiting for a verification of a RACP indication. */
    STATE_SOCP_RESPONSE_PENDING,
    STATE_ASCP_RESPONSE_PENDING
} gls_state_t;

static gls_state_t      m_gls_state;                                    /**< Current communication state. */
static uint8_t          m_racp_proc_operator;                           /**< Operator of current request. */
static uint8_t          m_racp_proc_record_ndx;                         /**< Current record index. */
static uint8_t          m_racp_proc_records_reported;                   /**< Number of reported records. */
static uint8_t          m_racp_proc_records_reported_since_txcomplete;  /**< Number of reported records since last TX_COMPLETE event. */
static ble_racp_value_t m_pending_racp_response;                        /**< RACP response to be sent. */
static uint8_t          m_pending_racp_response_operand[2];             /**< Operand of RACP response to be sent. */


ble_date_time_t start_session_time_def = {2012, 10, 5, 13, 27, 56};

#define CURRENT_TIME_YEAR_POS_LSB 0
#define CURRENT_TIME_YEAR_POS_MSB 1
#define CURRENT_TIME_MONTH_POS    2
#define CURRENT_TIME_DAY_POS      3
#define CURRENT_TIME_HOUR_POS     4
#define CURRENT_TIME_MINUTE_POS   5
#define CURRENT_TIME_SECONDS_POS  6

/**@brief Specific Operation Control Point opcodes. */
#define SOCP_OPCODE_RESERVED      0  /**< Specific Operation Control Point opcode - Reserved for future use. */

#define SOCP_WRITE_CGM_COMMUNICATION_INTERVAL         0x01
#define SOCP_READ_CGM_COMMUNICATION_INTERVAL          0x02
#define SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE 0x03
#define SOCP_WRITE_GLUCOSE_CALIBRATION_VALUE          0x04
#define SOCP_READ_GLUCOSE_CALIBRATION_VALUE           0x05
#define SOCP_READ_GLUCOSE_CALIBRATION_VALUE_RESPONSE  0x06
#define SOCP_WRITE_PATIENT_HIGH_ALERT_LEVEL           0x07
#define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL            0x08
#define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE   0x09
#define SOCP_WRITE_PATIENT_LOW_ALERT_LEVEL            0x0A
#define SOCP_READ_PATIENT_LOW_ALERT_LEVEL             0x0B
#define SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE    0x0C
#define SOCP_START_THE_SESSION                        0x0D
#define SOCP_STOP_THE_SESSION                         0x0E
#define SOCP_RESPONSE_CODE                            0x0F

#define SOCP_RSP_RESERVED_FOR_FUTURE_USE              0x00
#define SOCP_RSP_SUCCESS                              0x01
#define SOCP_RSP_OP_CODE_NOT_SUPPORTED                0x02
#define SOCP_RSP_INVALID_OPERAND                      0x03
#define SOCP_RSP_PROCEDURE_NOT_COMPLETED              0x04
#define SOCP_RSP_AUTHORIZATION_FAILED                 0x05

#define ASCP_REQUEST_FOR_AUTHORIZATION                0x01
#define ASCP_SET_NEW_AUTHORIZATION_CODE               0x02
#define ASCP_SET_NEW_PRIMARY_COLLECTOR                0x03

#define ASCP_AUTHORIZATION_RESPONSE_CODE              0x04

#define ASCP_RSP_SUCCESS                              0x01
#define ASCP_RSP_OP_CODE_NOT_SUPPORTED                0x02
#define ASCP_RSP_INVALID_OPERAND                      0x03
#define ASCP_RSP_AUTHORIZATION_FAILED                 0x04

#define ANNUNCIATION_PATIENT_HIGH_ALERT               0x1000
#define ANNUNCIATION_PATIENT_LOW_ALERT                0x2000

#define MAX_PATIENT_HIGH_ALERT                        1000


void ble_socp_decode(uint8_t data_len, uint8_t * p_data, ble_socp_value_t * p_socp_val)
{
    p_socp_val->opcode      = 0xFF;
    p_socp_val->operand_len = 0;
    p_socp_val->p_operand   = NULL;

    if (data_len > 0)
    {
        p_socp_val->opcode = p_data[0];
    }
    if (data_len > 1)
    {
        p_socp_val->operand_len = data_len - 1;
        p_socp_val->p_operand   = &p_data[1];  //lint !e416
    }
}


uint8_t ble_socp_encode(const ble_socp_rsp_t * p_socp_rsp, uint8_t * p_data)
{
    uint8_t len = 0;
    int     i;

    
    if (p_data != NULL)
    {
        p_data[len++] = p_socp_rsp->opcode;

        if (
            (p_socp_rsp->opcode != SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE) &&
            (p_socp_rsp->opcode != SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE)   &&
            (p_socp_rsp->opcode != SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE)
           )
        {
            p_data[len++] = p_socp_rsp->req_opcode;
            p_data[len++] = p_socp_rsp->rsp_code;
        }
        for (i = 0; i < p_socp_rsp->size_val; i++)
        {
            p_data[len++] = p_socp_rsp->resp_val[i];
        }
    }

    return len;
}


/**@brief Function for setting the GLS communication state.
 *
 * @param[in]   new_state  New communication state.
 */
static void state_set(gls_state_t new_state)
{
    m_gls_state = new_state;
}


/**@brief Function for setting next sequence number by reading the last record in the data base.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
static uint32_t next_sequence_number_set(void)
{
    uint16_t       num_records;
    ble_cgms_rec_t rec;

    num_records = ble_cgms_db_num_records_get();
    if (num_records > 0)
    {
        // Get last record
        uint32_t err_code = ble_cgms_db_record_get(num_records - 1, &rec);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}


/**@brief Function for encoding a Glucose measurement.
 *
 * @param[in]   p_meas            Measurement to be encoded.
 * @param[out]  p_encoded_buffer  Pointer to buffer where the encoded measurement is to be stored.
 *
 * @return      Size of encoded measurement.
 */
static uint8_t gls_meas_encode(const ble_cgms_meas_t * p_meas, uint8_t * p_encoded_buffer)
{
    uint8_t len = 1;

    p_encoded_buffer[len++] = (uint8_t)(p_meas->glucose_concentration);
    p_encoded_buffer[len++] = (uint8_t)(p_meas->glucose_concentration) >> 8;
    p_encoded_buffer[len++] = (uint8_t)(p_meas->time_offset);
    p_encoded_buffer[len++] = (uint8_t)(p_meas->time_offset) >> 8;

    if (p_meas->sensor_status_annunciation != 0)
    {
        p_encoded_buffer[len++] = (uint8_t)(p_meas->sensor_status_annunciation);
        p_encoded_buffer[len++] = (uint8_t)(p_meas->sensor_status_annunciation) >> 8;
    }
    p_encoded_buffer[0] = len - 1;
    return len;
}


/**@brief Function for adding a characteristic for the glucose measurement.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t glucose_measurement_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    ble_cgms_rec_t      initial_gls_rec_value;
    uint8_t             encoded_gls_meas[MAX_GLM_LEN];
    uint8_t             num_recs;

    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_MEASUREMENT_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));
    memset(&initial_gls_rec_value, 0, sizeof(initial_gls_rec_value));

    num_recs = ble_cgms_db_num_records_get();
    if (num_recs > 0)
    {
        uint32_t err_code = ble_cgms_db_record_get(num_recs - 1, &initial_gls_rec_value);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = gls_meas_encode(&initial_gls_rec_value.meas, encoded_gls_meas);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = MAX_GLM_LEN;
    attr_char_value.p_value   = encoded_gls_meas;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->glm_handles);
}


uint8_t encode_feature_location_type(uint8_t * p_out_buffer, ble_cgms_features_t * p_in_feature)
{
    uint8_t len = 0;

    p_out_buffer[len++] = (uint8_t)(p_in_feature->features);
    p_out_buffer[len++] = (uint8_t)((p_in_feature->features) >> 8);
    p_out_buffer[len++] = (p_in_feature->sample_location << 4) | (p_in_feature->type & 0x0F);

    return len;
}


/**@brief Function for adding a characteristic for the glucose feature.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t glucose_feature_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             encoded_initial_feature[3];

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read  = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf        = NULL;
    char_md.p_user_desc_md   = NULL;
    char_md.p_cccd_md        = NULL;
    char_md.p_sccd_md        = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_FEATURES_CHAR);
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    encode_feature_location_type(encoded_initial_feature, &(p_cgms->feature_location_type));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 3;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = 3;
    attr_char_value.p_value   = encoded_initial_feature;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->glf_handles);
}


/**@brief Function for adding a characteristic for the record access control point.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t record_access_control_point_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
  
    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.indicate = 1;
    char_md.char_props.write    = 1;
    char_md.p_char_user_desc    = NULL;
    char_md.p_char_pf           = NULL;
    char_md.p_user_desc_md      = NULL;
    char_md.p_cccd_md           = &cccd_md;
    char_md.p_sccd_md           = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_RECORD_ACCESS_CONTROL_POINT_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_L2CAP_MTU_DEF;
    attr_char_value.p_value   = 0;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->racp_handles);
}


/**@brief Function for adding a characteristic for the Status.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
/*static uint32_t _char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = NULL;
    char_md.p_sccd_md         = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_STATUS_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = 0;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = BLE_L2CAP_MTU_DEF;
    attr_char_value.p_value      = 0;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->racp_handles);
}*/


/**@brief Function for adding a characteristic for the session start time.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t sst_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             encoded_initial_start_session_time[SIZE_START_TIME_SESSION];

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read  = 1;    
    char_md.char_props.write = 1;
    char_md.p_char_user_desc = NULL;
    char_md.p_char_pf        = NULL;
    char_md.p_user_desc_md   = NULL;
    char_md.p_cccd_md        = NULL;
    char_md.p_sccd_md        = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_SESSION_START_TIME_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    ble_date_time_encode(&start_session_time_def, encoded_initial_start_session_time);

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = SIZE_START_TIME_SESSION;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = SIZE_START_TIME_SESSION;
    attr_char_value.p_value   = encoded_initial_start_session_time;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->cgmsst_handles);
}


/**@brief Function for adding a characteristic for the Application Security Point.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t asp_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.indicate = 1;
    char_md.char_props.write    = 1;
    char_md.p_char_user_desc    = NULL;
    char_md.p_char_pf           = NULL;
    char_md.p_user_desc_md      = NULL;
    char_md.p_cccd_md           = &cccd_md;
    char_md.p_sccd_md           = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_APPLICATION_SECURITY_POINT_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);

    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 0;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_L2CAP_MTU_DEF;
    attr_char_value.p_value   = 0;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->asp_handles);
}


/**@brief Function for adding a characteristic for the Specific Operations Control Point.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS if characteristic was successfully added, otherwise an error code.
 */
static uint32_t socp_char_add(ble_cgms_t * p_cgms)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.indicate = 1;
    char_md.char_props.write    = 1;
    char_md.p_char_user_desc    = NULL;
    char_md.p_char_pf           = NULL;
    char_md.p_user_desc_md      = NULL;
    char_md.p_cccd_md           = &cccd_md;
    char_md.p_sccd_md           = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, CGM_SPECIFIC_OPS_CONTROL_POINT_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);

    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = 0;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = BLE_L2CAP_MTU_DEF;
    attr_char_value.p_value      = 0;

    return sd_ble_gatts_characteristic_add(p_cgms->service_handle,
                                           &char_md,
                                           &attr_char_value,
                                           &p_cgms->socp_handles);
}


uint32_t ble_cgms_init(ble_cgms_t * p_cgms, const ble_cgm_init_t * p_gls_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

    // Initialize data base
    err_code = ble_cgms_db_init();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = next_sequence_number_set();
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Initialize service structure
    p_cgms->evt_handler             = p_gls_init->evt_handler;
    p_cgms->error_handler           = p_gls_init->error_handler;
    p_cgms->feature_location_type   = p_gls_init->feature_location_type;
    p_cgms->authorization_str_len   = p_gls_init->authorization_str_len;
    p_cgms->is_session_started      = false;
    p_cgms->is_collector_authorized = false;
    p_cgms->conn_handle             = BLE_CONN_HANDLE_INVALID;

    memcpy(p_cgms->authorization_string,
           p_gls_init->authorization_string,
           p_gls_init->authorization_str_len);

    // Initialize global variables
    state_set(STATE_NO_COMM);
    m_racp_proc_records_reported_since_txcomplete = 0;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_CGM_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_cgms->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add glucose measurement characteristic
    err_code = glucose_measurement_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add glucose measurement feature characteristic
    err_code = glucose_feature_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add record control access point characteristic
    err_code = record_access_control_point_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Start Session Time characteristic
    err_code = sst_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Application Security Point characteristic
    err_code = asp_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Specific Operations Control Point characteristic
    err_code = socp_char_add(p_cgms);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}


/**@brief Function for sending response from Specific Operation Control Point.
 *
 * @param[in]   p_cgms        Service instance.
 * @param[in]   p_racp_val   RACP value to be sent.
 */
static void racp_send(ble_cgms_t * p_cgms, ble_racp_value_t * p_racp_val)
{
    uint32_t               err_code;
    uint8_t                encoded_resp[25];
    uint8_t                len;
    uint16_t               hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    if (
        (m_gls_state != STATE_RACP_RESPONSE_PENDING)
        &&
        (m_racp_proc_records_reported_since_txcomplete > 0)
       )
    {
        state_set(STATE_RACP_RESPONSE_PENDING);
        return;
    }

    // Send indication
    len     = ble_racp_encode(p_racp_val, encoded_resp);
    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_cgms->racp_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_INDICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &hvx_len;
    hvx_params.p_data = encoded_resp;

    err_code = sd_ble_gatts_hvx(p_cgms->conn_handle, &hvx_params);

    // Error handling
    if ((err_code == NRF_SUCCESS) && (hvx_len != len))
    {
        err_code = NRF_ERROR_DATA_SIZE;
    }
    switch (err_code)
    {
        case NRF_SUCCESS:
            // Wait for HVC event
            state_set(STATE_RACP_RESPONSE_IND_VERIF);
            break;

        case BLE_ERROR_NO_TX_BUFFERS:
            // Wait for TX_COMPLETE event to retry transmission
            state_set(STATE_RACP_RESPONSE_PENDING);
            break;

        case NRF_ERROR_INVALID_STATE:
            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;

        default:
            // Report error to application
            if (p_cgms->error_handler != NULL)
            {
                p_cgms->error_handler(err_code);
            }

            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;
    }
}


/**@brief Function for sending a RACP response containing a Response Code Op Code and Response Code Value.
 *
 * @param[in]   p_cgms    Service instance.
 * @param[in]   opcode   RACP Op Code.
 * @param[in]   value    RACP Response Code Value.
 */
static void racp_response_code_send(ble_cgms_t * p_cgms, uint8_t opcode, uint8_t value)
{
    m_pending_racp_response.opcode      = RACP_OPCODE_RESPONSE_CODE;
    m_pending_racp_response.operator    = RACP_OPERATOR_NULL;
    m_pending_racp_response.operand_len = 2;
    m_pending_racp_response.p_operand   = m_pending_racp_response_operand;

    m_pending_racp_response_operand[0] = opcode;
    m_pending_racp_response_operand[1] = value;

    racp_send(p_cgms, &m_pending_racp_response);
}


/**@brief Function for sending glucose measurement/context.
 *
 * @param[in]   p_cgms   Service instance.
 * @param[in]   p_rec   Measurement to be sent.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
uint32_t glucose_meas_send(ble_cgms_t * p_cgms, ble_cgms_rec_t * p_rec, uint8_t * p_count)
{
    uint32_t               err_code;
    uint8_t                encoded_glm[MAX_GLM_LEN];
    uint16_t               len     = 0;
    uint16_t               hvx_len = MAX_GLM_LEN;
    int                    i;
    ble_gatts_hvx_params_t hvx_params;

    for (i = 0; i < *p_count; i++)
    {
        len += gls_meas_encode(&(p_rec[i].meas), (encoded_glm + len));
        if (len >= hvx_len)
        {
            break;
        }
    }
    *p_count = i;
    hvx_len  = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_cgms->glm_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &hvx_len;
    hvx_params.p_data = encoded_glm;

    err_code = sd_ble_gatts_hvx(p_cgms->conn_handle, &hvx_params);
    if (err_code == NRF_SUCCESS)
    {
        if (hvx_len != len)
        {
            err_code = NRF_ERROR_DATA_SIZE;
        }
        else
        {
            // Measurement successfully sent
            m_racp_proc_records_reported++;
            m_racp_proc_records_reported_since_txcomplete++;
        }
    }

    return err_code;
}


/**@brief Function for responding to the ALL operation.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t racp_report_records_all(ble_cgms_t * p_cgms)
{
    uint16_t total_records = ble_cgms_db_num_records_get();
    uint16_t cur_nb_rec;
    uint8_t  i;
    uint8_t  nb_rec_to_send;

    if (m_racp_proc_record_ndx >= total_records)
    {
        state_set(STATE_NO_COMM);
    }
    else
    {
        uint32_t       err_code;
        ble_cgms_rec_t rec[NB_MAX_RECORD_PER_NOTIFCATIONS];

        cur_nb_rec = total_records - m_racp_proc_record_ndx;
        if (cur_nb_rec > NB_MAX_RECORD_PER_NOTIFCATIONS)
        {
            cur_nb_rec = NB_MAX_RECORD_PER_NOTIFCATIONS;
        }
        nb_rec_to_send = (uint8_t)cur_nb_rec;
        for (i = 0; i < cur_nb_rec; i++)
        {
            err_code = ble_cgms_db_record_get(m_racp_proc_record_ndx + i, &(rec[i]));
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }
        err_code = glucose_meas_send(p_cgms, rec, &nb_rec_to_send);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
        m_racp_proc_record_ndx += cur_nb_rec;
    }

    return NRF_SUCCESS;
}


/**@brief Function for responding to the FIRST or the LAST operation.
 *
 * @param[in]   p_cgms   Service instance.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t racp_report_records_first_last(ble_cgms_t * p_cgms)
{
    uint32_t       err_code;
    ble_cgms_rec_t rec;
    uint16_t       total_records;
    uint8_t        nb_rec_to_send = 1;

    total_records = ble_cgms_db_num_records_get();

    if ((m_racp_proc_records_reported != 0) || (total_records == 0))
    {
        state_set(STATE_NO_COMM);
    }
    else
    {
        if (m_racp_proc_operator == RACP_OPERATOR_FIRST)
        {
            err_code = ble_cgms_db_record_get(0, &rec);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }
        else if (m_racp_proc_operator == RACP_OPERATOR_LAST)
        {
            err_code = ble_cgms_db_record_get(total_records - 1, &rec);
            if (err_code != NRF_SUCCESS)
            {
                return err_code;
            }
        }

        err_code = glucose_meas_send(p_cgms, &rec, &nb_rec_to_send);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    return NRF_SUCCESS;
}


/**@brief Function for informing that the REPORT RECORDS procedure is completed.
 *
 * @param[in]   p_cgms   Service instance.
 */
static void racp_report_records_completed(ble_cgms_t * p_cgms)
{
    uint8_t resp_code_value;

    if (m_racp_proc_records_reported > 0)
    {
        resp_code_value = RACP_RESPONSE_SUCCESS;
    }
    else
    {
        resp_code_value = RACP_RESPONSE_NO_RECORDS_FOUND;
    }

    racp_response_code_send(p_cgms, RACP_OPCODE_REPORT_RECS, resp_code_value);
}


/**@brief Function for the RACP report records procedure.
 *
 * @param[in]   p_cgms   Service instance.
 */
static void racp_report_records_procedure(ble_cgms_t * p_cgms)
{
    uint32_t err_code;

    while (m_gls_state == STATE_RACP_PROC_ACTIVE)
    {
        // Execute requested procedure
        switch (m_racp_proc_operator)
        {
            case RACP_OPERATOR_ALL:
                err_code = racp_report_records_all(p_cgms);
                break;

            case RACP_OPERATOR_FIRST:
            case RACP_OPERATOR_LAST:
                err_code = racp_report_records_first_last(p_cgms);
                break;

            default:
                // Report error to application
                if (p_cgms->error_handler != NULL)
                {
                    p_cgms->error_handler(NRF_ERROR_INTERNAL);
                }

                // Make sure state machine returns to the default state
                state_set(STATE_NO_COMM);
                return;
        }

        // Error handling
        switch (err_code)
        {
            case NRF_SUCCESS:
                if (m_gls_state == STATE_RACP_PROC_ACTIVE)
                {
                    m_racp_proc_record_ndx++;
                }
                else
                {
                    racp_report_records_completed(p_cgms);
                }
                break;

            case BLE_ERROR_NO_TX_BUFFERS:
                // Wait for TX_COMPLETE event to resume transmission
                return;

            case NRF_ERROR_INVALID_STATE:
                // Notification is probably not enabled. Ignore request.
                state_set(STATE_NO_COMM);
                return;

            default:
                // Report error to application
                if (p_cgms->error_handler != NULL)
                {
                    p_cgms->error_handler(err_code);
                }

                // Make sure state machine returns to the default state
                state_set(STATE_NO_COMM);
                return;
        }
    }
}


/**@brief Function for testing if the received request is to be executed.
 *
 * @param[in]    p_racp_request    Request to be checked.
 * @param[out]   p_response_code   Response code to be sent in case the request is rejected.
 *                                 RACP_RESPONSE_RESERVED is returned if the received message is
 *                                 to be rejected without sending a respone.
 *
 * @return       TRUE if the request is to be executed, FALSE if it is to be rejected.
 *               If it is to be rejected, p_response_code will contain the response code to be
 *               returned to the central.
 */
static bool is_request_to_be_executed(const ble_racp_value_t * p_racp_request,
                                      uint8_t                * p_response_code)
{
    *p_response_code = RACP_RESPONSE_RESERVED;

    if (p_racp_request->opcode == RACP_OPCODE_ABORT_OPERATION)
    {
        if (m_gls_state == STATE_RACP_PROC_ACTIVE)
        {
            if (p_racp_request->operator != RACP_OPERATOR_NULL)
            {
                *p_response_code = RACP_RESPONSE_INVALID_OPERATOR;
            }
            else if (p_racp_request->operand_len != 0)
            {
                *p_response_code = RACP_RESPONSE_INVALID_OPERAND;
            }
            else
            {
                *p_response_code = RACP_RESPONSE_SUCCESS;
            }
        }
        else
        {
            *p_response_code = RACP_RESPONSE_ABORT_FAILED;
        }
    }
    else if (m_gls_state != STATE_NO_COMM)
    {
        return false;
    }
    // supported opcodes
    else if ((p_racp_request->opcode == RACP_OPCODE_REPORT_RECS) ||
             (p_racp_request->opcode == RACP_OPCODE_REPORT_NUM_RECS))
    {
        switch (p_racp_request->operator)
        {
            // operators WITHOUT a filter
            case RACP_OPERATOR_ALL:
            case RACP_OPERATOR_FIRST:
            case RACP_OPERATOR_LAST:
                if (p_racp_request->operand_len != 0)
                {
                    *p_response_code = RACP_RESPONSE_INVALID_OPERAND;
                }
                break;

            // operators WITH a filter
            case RACP_OPERATOR_GREATER_OR_EQUAL:
                *p_response_code = RACP_RESPONSE_OPERATOR_UNSUPPORTED;
                // if (p_racp_request->p_operand[0] == OPERAND_FILTER_TYPE_SEQ_NUM)
                // {
                    // if (p_racp_request->operand_len != 3)
                    // {
                        // *p_response_code = RACP_RESPONSE_INVALID_OPERAND;
                    // }
                // }
                // else if (p_racp_request->p_operand[0] == OPERAND_FILTER_TYPE_FACING_TIME)
                // {
                    // *p_response_code = RACP_RESPONSE_OPERAND_UNSUPPORTED;
                // }
                // else
                // {
                    // *p_response_code = RACP_RESPONSE_INVALID_OPERAND;
                // }
                break;

            //  unsupported operators
            case RACP_OPERATOR_LESS_OR_EQUAL:
            case RACP_OPERATOR_RANGE:
                *p_response_code = RACP_RESPONSE_OPERATOR_UNSUPPORTED;
                 break;

            // invalid operators
            case RACP_OPERATOR_NULL:
            default:
                *p_response_code = RACP_RESPONSE_INVALID_OPERATOR;
                break;
        }
    }
    // unsupported opcodes
    else if (p_racp_request->opcode == RACP_OPCODE_DELETE_RECS)
    {
        *p_response_code = RACP_RESPONSE_OPCODE_UNSUPPORTED;
    }
    // unknown opcodes
    else
    {
        *p_response_code = RACP_RESPONSE_OPCODE_UNSUPPORTED;
    }
 
    // NOTE: The computation of the return value will change slightly when deferred write has been
    //       implemented in the stack.
    return (*p_response_code == RACP_RESPONSE_RESERVED);
}


/**@brief Function for processing a REPORT RECORDS request.
 *
 * @param[in]   p_cgms            Service instance.
 * @param[in]   p_racp_request   Request to be executed.
 */
static void report_records_request_execute(ble_cgms_t * p_cgms, ble_racp_value_t * p_racp_request)
{
    state_set(STATE_RACP_PROC_ACTIVE);

    m_racp_proc_record_ndx       = 0;
    m_racp_proc_operator         = p_racp_request->operator;
    m_racp_proc_records_reported = 0;

    racp_report_records_procedure(p_cgms);
}


/**@brief Function for processing a REPORT NUM RECORDS request.
 *
 * @param[in]   p_cgms            Service instance.
 * @param[in]   p_racp_request   Request to be executed.
 */
static void report_num_records_request_execute(ble_cgms_t       * p_cgms,
                                               ble_racp_value_t * p_racp_request)
{
    uint16_t total_records;
    uint16_t num_records;

    total_records = ble_cgms_db_num_records_get();
    num_records   = 0;

    if (p_racp_request->operator == RACP_OPERATOR_ALL)
    {
        num_records = total_records;
    }
    else if ((p_racp_request->operator == RACP_OPERATOR_FIRST) ||
             (p_racp_request->operator == RACP_OPERATOR_LAST))
    {
        if (total_records > 0)
        {
            num_records = 1;
        }
    }

    m_pending_racp_response.opcode      = RACP_OPCODE_NUM_RECS_RESPONSE;
    m_pending_racp_response.operator    = RACP_OPERATOR_NULL;
    m_pending_racp_response.operand_len = sizeof(uint16_t);
    m_pending_racp_response.p_operand   = m_pending_racp_response_operand;

    m_pending_racp_response_operand[0] = num_records & 0xFF;
    m_pending_racp_response_operand[1] = num_records >> 8;

    racp_send(p_cgms, &m_pending_racp_response);
}


/**@brief Function for handling a write event to the Record Access Control Point.
 *
 * @param[in]   p_cgms         Service instance.
 * @param[in]   p_evt_write   WRITE event to be handled.
 */
static void on_racp_value_write(ble_cgms_t * p_cgms, ble_gatts_evt_write_t * p_evt_write)
{
    ble_racp_value_t racp_request;
    uint8_t          response_code;

    // Decode request
    ble_racp_decode(p_evt_write->len, p_evt_write->data, &racp_request);

    // Check if request is to be executed
    if (is_request_to_be_executed(&racp_request, &response_code))
    {
        // Execute request
        if (racp_request.opcode == RACP_OPCODE_REPORT_RECS)
        {
            report_records_request_execute(p_cgms, &racp_request);
        }
        else if (racp_request.opcode == RACP_OPCODE_REPORT_NUM_RECS)
        {
            report_num_records_request_execute(p_cgms, &racp_request);
        }
    }
    else if (response_code != RACP_RESPONSE_RESERVED)
    {
        // Abort any running procedure
        state_set(STATE_NO_COMM);

        // Respond with error code
        racp_response_code_send(p_cgms, racp_request.opcode, response_code);
    }
    else
    {
        // Ignore request
    }
}


/**@brief Function for sending a response from the Specific Operation Control Point.
 *
 * @param[in]   p_cgms        Service instance.
 */
static void socp_send(ble_cgms_t * p_cgms)
{
    uint32_t               err_code;
    uint8_t                encoded_resp[25];
    uint8_t                len;
    uint16_t               hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    // Send indication
    len     = ble_socp_encode(&(p_cgms->socp_response), encoded_resp);
    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_cgms->socp_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_INDICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &hvx_len;
    hvx_params.p_data = encoded_resp;

    err_code = sd_ble_gatts_hvx(p_cgms->conn_handle, &hvx_params);

    // Error handling
    if ((err_code == NRF_SUCCESS) && (hvx_len != len))
    {
        err_code = NRF_ERROR_DATA_SIZE;
    }
    switch (err_code)
    {
        case NRF_SUCCESS:
            // Wait for HVC event
            state_set(STATE_RACP_RESPONSE_IND_VERIF);
            break;

        case BLE_ERROR_NO_TX_BUFFERS:
            // Wait for TX_COMPLETE event to retry transmission
            state_set(STATE_SOCP_RESPONSE_PENDING);
            break;

        case NRF_ERROR_INVALID_STATE:
            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;

        default:
            // Report error to application
            if (p_cgms->error_handler != NULL)
            {
                p_cgms->error_handler(err_code);
            }

            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;
    }
}


// #define SOCP_WRITE_CGM_COMMUNICATION_INTERVAL           0x01
// #define SOCP_READ_CGM_COMMUNICATION_INTERVAL            0x02
// #define SOCP_READ_CGM_COMMUNICATION_INTERVAL            0x03
// #define SOCP_WRITE_GLUCOSE_CALIBRATION_VALUE            0x04
// #define SOCP_READ_GLUCOSE_CALIBRATION_VALUE             0x05
// #define SOCP_READ_GLUCOSE_CALIBRATION_VALUE_RESPONSE    0x06
// #define SOCP_WRITE_PATIENT_HIGH_ALERT_LEVEL             0x07
// #define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL              0x08
// #define SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE     0x09
// #define SOCP_WRITE_PATIENT_LOW_ALERT_LEVEL              0x0A
// #define SOCP_READ_PATIENT_LOW_ALERT_LEVEL               0x0B
// #define SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE      0x0C
// #define SOCP_START_THE_SESSION                          0x0D
// #define SOCP_STOP_THE_SESSION                           0x0E
// #define SOCP_RESPONSE_CODE                              0x0F

// #define SOCP_RSP_RESERVED_FOR_FUTURE_USE                0x00
// #define SOCP_RSP_SUCCESS                                0x01
// #define SOCP_RSP_OP_CODE_NOT_SUPPORTED                  0x02
// #define SOCP_RSP_INVALID_OPERAND                        0x03
// #define SOCP_RSP_PROCEDURE_NOT_COMPLETED                0x04
// #define SOCP_RSP_AUTHORIZATION_FAILED                   0x05


/**@brief Function for handling a write event to the Specific Operation Control Point.
 *
 * @param[in]   p_cgms         Service instance.
 * @param[in]   p_evt_write   WRITE event to be handled.
 */
static void on_socp_value_write(ble_cgms_t * p_cgms, ble_gatts_evt_write_t * p_evt_write)
{
    ble_socp_value_t socp_request;
    ble_cgms_evt_t   evt;

    // Decode request
    ble_socp_decode(p_evt_write->len, p_evt_write->data, &socp_request);

    p_cgms->socp_response.opcode     = SOCP_RESPONSE_CODE;
    p_cgms->socp_response.req_opcode = socp_request.opcode;
    p_cgms->socp_response.rsp_code   = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
    p_cgms->socp_response.size_val   = 0;

    switch (socp_request.opcode)
    {
        case SOCP_WRITE_CGM_COMMUNICATION_INTERVAL:
            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            p_cgms->comm_interval          = socp_request.p_operand[0];
            evt.evt_type                   = BLE_GLS_EVT_WRITE_COMM_INTERVAL;
            p_cgms->evt_handler(p_cgms, &evt);
            break;

        case SOCP_READ_CGM_COMMUNICATION_INTERVAL:
            p_cgms->socp_response.opcode      = SOCP_READ_CGM_COMMUNICATION_INTERVAL_RESPONSE;
            p_cgms->socp_response.resp_val[0] = p_cgms->comm_interval;
            p_cgms->socp_response.size_val++;
            break;

        case SOCP_WRITE_GLUCOSE_CALIBRATION_VALUE:
            break;
        case SOCP_READ_GLUCOSE_CALIBRATION_VALUE:
            break;
        case SOCP_READ_GLUCOSE_CALIBRATION_VALUE_RESPONSE:
            break;

        case SOCP_WRITE_PATIENT_HIGH_ALERT_LEVEL:
        {
            uint16_t rcvd_high_alert;

            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            rcvd_high_alert                = socp_request.p_operand[1]<<8;
            rcvd_high_alert               |= socp_request.p_operand[0];
            if ( rcvd_high_alert > MAX_PATIENT_HIGH_ALERT)
            {
                p_cgms->socp_response.rsp_code = SOCP_RSP_INVALID_OPERAND;
            }
            else
            {
                p_cgms->patient_high_alert = rcvd_high_alert;
            }
            //evt.evt_type = BLE_GLS_EVT_WRITE_COMM_INTERVAL;
            p_cgms->evt_handler(p_cgms, &evt);
            break;
        }
        case SOCP_READ_PATIENT_HIGH_ALERT_LEVEL:
            p_cgms->socp_response.opcode   = SOCP_READ_PATIENT_HIGH_ALERT_LEVEL_RESPONSE;
            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            p_cgms->socp_response.resp_val[p_cgms->socp_response.size_val++] = (uint8_t)(p_cgms->patient_high_alert);
            p_cgms->socp_response.resp_val[p_cgms->socp_response.size_val++] = (uint8_t)(p_cgms->patient_high_alert >> 8);
            break;

        case SOCP_WRITE_PATIENT_LOW_ALERT_LEVEL:
            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            p_cgms->patient_low_alert      = socp_request.p_operand[1] << 8;
            p_cgms->patient_low_alert     |= socp_request.p_operand[0];
            //evt.evt_type = BLE_GLS_EVT_WRITE_COMM_INTERVAL;
            p_cgms->evt_handler(p_cgms, &evt);
            break;

        case SOCP_READ_PATIENT_LOW_ALERT_LEVEL:
            p_cgms->socp_response.opcode   = SOCP_READ_PATIENT_LOW_ALERT_LEVEL_RESPONSE;
            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            p_cgms->socp_response.resp_val[p_cgms->socp_response.size_val++] = (uint8_t)(p_cgms->patient_low_alert);
            p_cgms->socp_response.resp_val[p_cgms->socp_response.size_val++] = (uint8_t)(p_cgms->patient_low_alert >> 8);
            break;

        case SOCP_START_THE_SESSION:
            if (p_cgms->is_session_started)
            {
                p_cgms->socp_response.rsp_code = SOCP_RSP_PROCEDURE_NOT_COMPLETED;
            }
            else
            {
                evt.evt_type = BLE_GLS_EVT_START_SESSION;
                p_cgms->evt_handler(p_cgms, &evt);
                p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
                p_cgms->is_session_started     = true;
            }
            break;

        case SOCP_STOP_THE_SESSION:
            evt.evt_type = BLE_GLS_EVT_STOP_SESSION;
            p_cgms->evt_handler(p_cgms, &evt);
            p_cgms->socp_response.rsp_code = SOCP_RSP_SUCCESS;
            p_cgms->is_session_started     = false;
            break;

        default:
            p_cgms->socp_response.rsp_code = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
            break;
    }

    socp_send(p_cgms);
}


/**@brief Function for sending a response from the Specific Operation Control Point.
 *
 * @param[in]   p_cgms        Service instance.
 */
static void ascp_send(ble_cgms_t * p_cgms)
{
    uint32_t               err_code;
    uint8_t                encoded_resp[25];
    uint8_t                len;
    uint16_t               hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    // Send indication
    len     = ble_socp_encode(&(p_cgms->ascp_response), encoded_resp);
    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_cgms->asp_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_INDICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &hvx_len;
    hvx_params.p_data = encoded_resp;

    err_code = sd_ble_gatts_hvx(p_cgms->conn_handle, &hvx_params);

    // Error handling
    if ((err_code == NRF_SUCCESS) && (hvx_len != len))
    {
        err_code = NRF_ERROR_DATA_SIZE;
    }
    switch (err_code)
    {
        case NRF_SUCCESS:
            // Wait for HVC event
            state_set(STATE_RACP_RESPONSE_IND_VERIF);
            break;

        case BLE_ERROR_NO_TX_BUFFERS:
            // Wait for TX_COMPLETE event to retry transmission
            state_set(STATE_ASCP_RESPONSE_PENDING);
            break;

        case NRF_ERROR_INVALID_STATE:
            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;

        default:
            // Report error to application
            if (p_cgms->error_handler != NULL)
            {
                p_cgms->error_handler(err_code);
            }

            // Make sure state machine returns to the default state
            state_set(STATE_NO_COMM);
            break;
    }
}


/**@brief Function for handling a write event to the Application Security Control Point.
 *
 * @param[in]   p_cgms         Service instance.
 * @param[in]   p_evt_write   WRITE event to be handled.
 */
static void on_asp_value_write(ble_cgms_t * p_cgms, ble_gatts_evt_write_t * p_evt_write)
{
    ble_socp_value_t ascp_request;

    // Decode request
    ble_socp_decode(p_evt_write->len, p_evt_write->data, &ascp_request);

    p_cgms->ascp_response.opcode     = ASCP_AUTHORIZATION_RESPONSE_CODE;
    p_cgms->ascp_response.req_opcode = ascp_request.opcode;
    p_cgms->ascp_response.rsp_code   = ASCP_RSP_OP_CODE_NOT_SUPPORTED;
    p_cgms->ascp_response.size_val   = 0;

    switch (ascp_request.opcode)
    {
        case ASCP_REQUEST_FOR_AUTHORIZATION:
            if (p_cgms->authorization_str_len == ascp_request.operand_len)
            {
                int  i;
                bool is_authorized = true;

                for (i = 0; i < ascp_request.operand_len; i++)
                {
                    if (ascp_request.p_operand[i] != p_cgms->authorization_string[i])
                    {
                        is_authorized = false;
                        break;
                    }
                }
                if (is_authorized)
                {
                    p_cgms->ascp_response.rsp_code  = ASCP_RSP_SUCCESS;
                    p_cgms->is_collector_authorized = true;
                }
                else
                {
                    p_cgms->ascp_response.rsp_code  = ASCP_RSP_AUTHORIZATION_FAILED;
                    p_cgms->is_collector_authorized = false;
                }
            }
            else
            {
                p_cgms->ascp_response.rsp_code  = ASCP_RSP_AUTHORIZATION_FAILED;
                p_cgms->is_collector_authorized = false;
            }
            break;

        case ASCP_SET_NEW_AUTHORIZATION_CODE:
            if (p_cgms->is_collector_authorized)
            {
                if ((ascp_request.operand_len < MIN_AUTHO_STRING_LEN) ||(ascp_request.operand_len > MAX_AUTH_STR_LEN))
                {
                    p_cgms->ascp_response.rsp_code = ASCP_RSP_INVALID_OPERAND;
                }
                else
                {
                    p_cgms->ascp_response.rsp_code = ASCP_RSP_SUCCESS;
                    p_cgms->authorization_str_len  = ascp_request.operand_len;

                    memcpy(p_cgms->authorization_string,
                           ascp_request.p_operand,
                           ascp_request.operand_len);
                }
            }
            else
            {
                p_cgms->ascp_response.rsp_code = ASCP_RSP_AUTHORIZATION_FAILED;
            }
            break;

        case ASCP_SET_NEW_PRIMARY_COLLECTOR:
            if (p_cgms->is_collector_authorized)
            {
                p_cgms->ascp_response.rsp_code = ASCP_RSP_OP_CODE_NOT_SUPPORTED;
            }
            else
            {
                p_cgms->ascp_response.rsp_code = ASCP_RSP_AUTHORIZATION_FAILED;
            }
            break;

        default:
            p_cgms->ascp_response.rsp_code = SOCP_RSP_OP_CODE_NOT_SUPPORTED;
            break;
    }

    ascp_send(p_cgms);
}


/**@brief Function for handling the Glucose measurement CCCD write event.
 *
 * @param[in]   p_cgms         Service instance.
 * @param[in]   p_evt_write   WRITE event to be handled.
 */
static void on_glm_cccd_write(ble_cgms_t * p_cgms, ble_gatts_evt_write_t * p_evt_write)
{
    if (p_evt_write->len == 2)
    {
        // CCCD written, update notification state
        if (p_cgms->evt_handler != NULL)
        {
            ble_cgms_evt_t evt;

            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_GLS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_GLS_EVT_NOTIFICATION_DISABLED;
            }

            p_cgms->evt_handler(p_cgms, &evt);
        }
    }
}


/**@brief Function for handling the WRITE event.
 *
 * @details Handles WRITE events from the BLE stack.
 *
 * @param[in]   p_cgms      Glucose Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
static void on_write(ble_cgms_t * p_cgms, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_cgms->glm_handles.cccd_handle)
    {
        on_glm_cccd_write(p_cgms, p_evt_write);
    }
    else if (p_evt_write->handle == p_cgms->racp_handles.value_handle)
    {
        on_racp_value_write(p_cgms, p_evt_write);
    }
    else if (p_evt_write->handle == p_cgms->socp_handles.value_handle)
    {
        on_socp_value_write(p_cgms, p_evt_write);
    }
    else if (p_evt_write->handle == p_cgms->asp_handles.value_handle)
    {
        on_asp_value_write(p_cgms, p_evt_write);
    }

}


/**@brief Function for handling the TX_COMPLETE event.
 *
 * @details Handles TX_COMPLETE events from the BLE stack.
 *
 * @param[in]   p_cgms      Glucose Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
static void on_tx_complete(ble_cgms_t * p_cgms, ble_evt_t * p_ble_evt)
{
    m_racp_proc_records_reported_since_txcomplete = 0;

    if (m_gls_state == STATE_RACP_RESPONSE_PENDING)
    {
        racp_send(p_cgms, &m_pending_racp_response);
    }
    else if (m_gls_state == STATE_RACP_PROC_ACTIVE)
    {
        racp_report_records_procedure(p_cgms);
    }
    else if (m_gls_state == STATE_SOCP_RESPONSE_PENDING)
    {
        socp_send(p_cgms);
    }
    else if (m_gls_state == STATE_ASCP_RESPONSE_PENDING)
    {
        ascp_send(p_cgms);
    }
}


/**@brief Function for handling the HVC event.
 *
 * @details Handles HVC events from the BLE stack.
 *
 * @param[in]   p_cgms      Glucose Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
static void on_hvc(ble_cgms_t * p_cgms, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_hvc_t * p_hvc = &p_ble_evt->evt.gatts_evt.params.hvc;

    if (p_hvc->handle == p_cgms->racp_handles.value_handle)
    {
        if (m_gls_state == STATE_RACP_RESPONSE_IND_VERIF)
        {
            // Indication has been acknowledged. Return to default state.
            state_set(STATE_NO_COMM);
        }
        else
        {
            // We did not expect this event in this state. Report error to application.
            if (p_cgms->error_handler != NULL)
            {
                p_cgms->error_handler(NRF_ERROR_INVALID_STATE);
            }
        }
    }
}


void ble_cgms_on_ble_evt(ble_cgms_t * p_cgms, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            p_cgms->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            state_set(STATE_NO_COMM);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            p_cgms->conn_handle             = BLE_CONN_HANDLE_INVALID;
            p_cgms->is_collector_authorized = false;
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_cgms, p_ble_evt);
            break;

        case BLE_EVT_TX_COMPLETE:
            on_tx_complete(p_cgms, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVC:
            on_hvc(p_cgms, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_cgms_glucose_new_meas(ble_cgms_t * p_cgms, ble_cgms_rec_t * p_rec)
{
    uint32_t err_code       = NRF_SUCCESS;
    uint8_t  nb_rec_to_send = 1;

    if (p_rec->meas.glucose_concentration > p_cgms->patient_high_alert)
    {
        p_rec->meas.sensor_status_annunciation |= ANNUNCIATION_PATIENT_HIGH_ALERT;
    }

    if (p_rec->meas.glucose_concentration < p_cgms->patient_low_alert)
    {
        p_rec->meas.sensor_status_annunciation |= ANNUNCIATION_PATIENT_LOW_ALERT;
    }
    err_code = ble_cgms_db_record_add(p_rec);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if ((p_cgms->conn_handle != BLE_CONN_HANDLE_INVALID) && (p_cgms->comm_interval != 0))
    {
        err_code = glucose_meas_send(p_cgms, p_rec, &nb_rec_to_send);
    }
    return err_code;
}
