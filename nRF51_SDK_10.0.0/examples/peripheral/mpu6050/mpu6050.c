/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
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

#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_twi.h"
#include "app_error.h"
#include "mpu6050.h"

/* Gyro sensitivities in degrees/s */
#define MPU6050_GYRO_SENS_250		((float) 131)
#define MPU6050_GYRO_SENS_500		((float) 65.5)
#define MPU6050_GYRO_SENS_1000		((float) 32.8)
#define MPU6050_GYRO_SENS_2000		((float) 16.4)

/* Acce sensitivities in g/s */
#define MPU6050_ACCE_SENS_2			((float) 16384)
#define MPU6050_ACCE_SENS_4			((float) 8192)
#define MPU6050_ACCE_SENS_8			((float) 4096)
#define MPU6050_ACCE_SENS_16		((float) 2048)

#define ADDRESS_WHO_AM_I          (0x75U) // !< WHO_AM_I register identifies the device. Expected value is 0x68.
//#define ADDRESS_SIGNAL_PATH_RESET (0x68U) // !<

static const uint8_t expected_who_am_i = 0x68U; // !< Expected value to get from WHO_AM_I register.
//static uint8_t       m_device_address;          // !< Device address in bits [7:1]

void mpu6050_init(const nrf_drv_twi_t * twi)
{
	uint8_t inData[7]={0x00,												//0x19
								0x00,												//0x1A
								0x03,												//0x6B
								0x10,												//0x1B
								0x00,												//0x6A
								0x32,												//0x37
								0x01};											//0x38
	uint8_t acc = 0x00;	

    // Do a reset on signal paths
    uint8_t reset_value = 0x04U | 0x02U | 0x01U; // Resets gyro, accelerometer and temperature sensor signal paths.
    mpu6050_register_write(twi, MPU6050_RA_SIGNAL_PATH_RESET, reset_value);

    // Read and verify product ID
    mpu6050_verify_product_id(twi);
	
		//设置GYRO
	mpu6050_register_write(twi, MPU6050_RA_SMPLRT_DIV, 	inData[0]); //设置采样率    -- SMPLRT_DIV = 0  Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
	mpu6050_register_write(twi, MPU6050_RA_CONFIG, 		inData[1]); //CONFIG        -- EXT_SYNC_SET 0 (禁用晶振输入脚) ; default DLPF_CFG = 0 => (低通滤波)ACC bandwidth = 260Hz  GYRO bandwidth = 256Hz)
	mpu6050_register_write(twi, MPU6050_RA_PWR_MGMT_1, 	inData[2]); //PWR_MGMT_1    -- SLEEP 0; CYCLE 0; TEMP_DIS 0; CLKSEL 3 (PLL with Z Gyro reference)
	mpu6050_register_write(twi, MPU6050_RA_GYRO_CONFIG, inData[3]); //gyro配置 量程  0-1000度每秒
	mpu6050_register_write(twi, MPU6050_RA_USER_CTRL,	inData[4]);	// 0x6A的 I2C_MST_EN  设置成0  默认为0 6050  时能主IIC 	
	// 0x37的 推挽输出，高电平中断，一直输出高电平直到中断清除，任何读取操作都清除中断 使能 pass through 功能 直接在6050 读取5883数据
	mpu6050_register_write(twi, MPU6050_RA_INT_PIN_CFG, inData[5]);	
	mpu6050_register_write(twi, MPU6050_RA_INT_ENABLE,  inData[6]); //时能data ready  引脚	
								
	//设置 ACC
	mpu6050_register_write(twi, MPU6050_RA_ACCEL_CONFIG,acc);       //ACC设置  量程 +-2G s 	
}

bool mpu6050_verify_product_id(const nrf_drv_twi_t * twi)
{
    uint8_t who_am_i;

    if (NRF_SUCCESS != mpu6050_register_read(twi, MPU6050_RA_WHO_AM_I, &who_am_i, 1))
    {
        if (who_am_i != expected_who_am_i)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return false;
    }
}

ret_code_t mpu6050_register_write(const nrf_drv_twi_t * twi, uint8_t register_address, uint8_t value)
{
    uint8_t w2_data[2];
	//uint32_t err_code;
	
    w2_data[0] = register_address;
    w2_data[1] = value;
    //return twi_master_transfer(m_device_address, w2_data, 2, TWI_ISSUE_STOP);
	return nrf_drv_twi_tx(twi, MPU6050_DEFAULT_ADDRESS, w2_data, sizeof(w2_data), false);
	//APP_ERROR_CHECK(err_code);
}

ret_code_t mpu6050_register_read(const nrf_drv_twi_t * twi, uint8_t register_address, uint8_t * destination, uint8_t number_of_bytes)
{
    //bool transfer_succeeded;
	uint32_t err_code;
	
    err_code = nrf_drv_twi_tx(twi, MPU6050_DEFAULT_ADDRESS, &register_address, 	1, 					false);
    err_code = nrf_drv_twi_rx(twi, MPU6050_DEFAULT_ADDRESS, destination, 		number_of_bytes, 	false);
	
	return err_code;
}

void MPU6050_ReadAcc(const nrf_drv_twi_t * twi, int16_t *pACC_X , int16_t *pACC_Y , int16_t *pACC_Z )
{
    
	uint8_t buf[6];
    
	mpu6050_register_read(twi, MPU6050_RA_ACCEL_XOUT_H, buf, 6);
  *pACC_X = (buf[0] << 8) | buf[1];
	if(*pACC_X & 0x8000) *pACC_X-=65536;
		
	*pACC_Y= (buf[2] << 8) | buf[3];
  if(*pACC_Y & 0x8000) *pACC_Y-=65536;
	
  *pACC_Z = (buf[4] << 8) | buf[5];
	if(*pACC_Z & 0x8000) *pACC_Z-=65536;
}

/*lint --flb "Leave library region" */
