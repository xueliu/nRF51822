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

#include "twi_master.h"
#include "mpu6050.h"

/*lint ++flb "Enter library region" */

#define ADDRESS_WHO_AM_I (0x75U) //!< WHO_AM_I register identifies the device. Expected value is 0x68.
#define ADDRESS_SIGNAL_PATH_RESET (0x68U) //!< 

static const uint8_t expected_who_am_i = 0x68U; //!< Expected value to get from WHO_AM_I register.
static uint8_t m_device_address; //!< Device address in bits [7:1]

bool mpu6050_init(uint8_t device_address)
{   
  bool transfer_succeeded = true;
	
	uint8_t inData[7]={0x00,												//0x19
								0x00,												//0x1A
								0x03,												//0x6B
								0x10,												//0x1B
								0x00,												//0x6A
								0x32,												//0x37
								0x01};											//0x38
	uint8_t acc = 0x00;							
								
  m_device_address = (uint8_t)(device_address << 1);

  // Do a reset on signal paths
  uint8_t reset_value = 0x04U | 0x02U | 0x01U; // Resets gyro, accelerometer and temperature sensor signal paths
  transfer_succeeded &= mpu6050_register_write(ADDRESS_SIGNAL_PATH_RESET, reset_value);
  
	
	//设置GYRO
	mpu6050_register_write(0x19 , inData[0]); //设置采样率    -- SMPLRT_DIV = 0  Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
	mpu6050_register_write(0x1A , inData[1]); //CONFIG        -- EXT_SYNC_SET 0 (禁用晶振输入脚) ; default DLPF_CFG = 0 => (低通滤波)ACC bandwidth = 260Hz  GYRO bandwidth = 256Hz)
	mpu6050_register_write(0x6B , inData[2]); //PWR_MGMT_1    -- SLEEP 0; CYCLE 0; TEMP_DIS 0; CLKSEL 3 (PLL with Z Gyro reference)
	mpu6050_register_write(0x1B , inData[3]); //gyro配置 量程  0-1000度每秒
	mpu6050_register_write(0x6A , inData[4]);	// 0x6A的 I2C_MST_EN  设置成0  默认为0 6050  时能主IIC 	
	// 0x37的 推挽输出，高电平中断，一直输出高电平直到中断清除，任何读取操作都清除中断 使能 pass through 功能 直接在6050 读取5883数据
	mpu6050_register_write(0x37,  inData[5]);	
	mpu6050_register_write(0x38,  inData[6]); //时能data ready  引脚	
								
	//设置 ACC
  mpu6050_register_write(0x1C,acc);         				//ACC设置  量程 +-2G s 		

								
  // Read and verify product ID
  transfer_succeeded &= mpu6050_verify_product_id();

  return transfer_succeeded;
}

bool mpu6050_verify_product_id(void)
{
  uint8_t who_am_i;
  
  if (mpu6050_register_read(ADDRESS_WHO_AM_I, &who_am_i, 1))
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

bool mpu6050_register_write(uint8_t register_address, uint8_t value)
{	
	uint8_t w2_data[2];
	
	w2_data[0] = register_address;
	w2_data[1] = value;
  return twi_master_transfer(m_device_address, w2_data, 2, TWI_ISSUE_STOP);	  
}

bool mpu6050_register_read(uint8_t register_address, uint8_t *destination, uint8_t number_of_bytes)
{
  bool transfer_succeeded;
  transfer_succeeded = twi_master_transfer(m_device_address, &register_address, 1, TWI_DONT_ISSUE_STOP);
  transfer_succeeded &= twi_master_transfer(m_device_address|TWI_READ_BIT, destination, number_of_bytes, TWI_ISSUE_STOP);
  return transfer_succeeded;
}


void MPU6050_ReadGyro(int16_t *pGYRO_X , int16_t *pGYRO_Y , int16_t *pGYRO_Z )
{
  uint8_t buf[6];    		
//	uint8_t addr= MPU6050_ADDRESS << 1;
	
  mpu6050_register_read(MPU6050_GYRO_OUT,  buf, 6);
	
  *pGYRO_X = (buf[0] << 8) | buf[1];
	if(*pGYRO_X & 0x8000) *pGYRO_X-=65536;
		
	*pGYRO_Y= (buf[2] << 8) | buf[3];
  if(*pGYRO_Y & 0x8000) *pGYRO_Y-=65536;
	
  *pGYRO_Z = (buf[4] << 8) | buf[5];
	if(*pGYRO_Z & 0x8000) *pGYRO_Z-=65536;
}
			   

void MPU6050_ReadAcc( int16_t *pACC_X , int16_t *pACC_Y , int16_t *pACC_Z )
{
    
	uint8_t buf[6];    		
//	uint8_t addr= MPU6050_ADDRESS << 1;

    
	mpu6050_register_read(MPU6050_ACC_OUT, buf, 6);
  *pACC_X = (buf[0] << 8) | buf[1];
	if(*pACC_X & 0x8000) *pACC_X-=65536;
		
	*pACC_Y= (buf[2] << 8) | buf[3];
  if(*pACC_Y & 0x8000) *pACC_Y-=65536;
	
  *pACC_Z = (buf[4] << 8) | buf[5];
	if(*pACC_Z & 0x8000) *pACC_Z-=65536;
}


/*lint --flb "Leave library region" */ 
