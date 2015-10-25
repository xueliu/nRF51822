/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
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

#ifndef NRF_DRV_PPI_H
#define NRF_DRV_PPI_H

/*lint ++flb "Enter library region" */
#include "nrf_error.h"
#include "nrf_ppi.h"
#include <stdbool.h>
#include <stdint.h>

/** @file
 *
 * @addtogroup nrf_ppi PPI HAL and driver
 * @ingroup nrf_drivers
 * @brief Programmable Peripheral Interconnect (PPI) APIs.
 *
 * @details The PPI HAL provides basic APIs for accessing the registers of the PPI. 
 * The PPI driver provides APIs on a higher level.
 *
 * @defgroup lib_driver_ppi PPI driver
 * @{
 * @ingroup  nrf_ppi
 *
 * @brief Programmable Peripheral Interconnect (PPI) driver.
 */

#ifdef SOFTDEVICE_PRESENT

#include "nrf_sd_def.h"

#else

#define NRF_PPI_RESTRICTED              0                         /**< 1 if PPI peripheral is restricted, 0 otherwise. */
#define NRF_PPI_ALL_APP_CHANNELS_MASK   ((uint32_t)0xFFF0FFFFuL)  /**< All PPI channels available to the application. */
#define NRF_PPI_PROG_APP_CHANNELS_MASK  ((uint32_t)0x0000FFFFuL)  /**< Programmable PPI channels available to the application. */
#define NRF_PPI_ALL_APP_GROUPS_MASK     ((uint32_t)0x0000000FuL)  /**< All PPI groups available to the application. */

#endif // SOFTDEVICE_PRESENT

/**@brief Function for initializing PPI module.
 *
 * @retval     NRF_SUCCESS             If the module was successfully initialized.
 * @retval     NRF_ERROR_INVALID_STATE If the module has already been initialized.
 */
uint32_t nrf_drv_ppi_init(void);

/**@brief Function for uninitializing the PPI module. 
 *
 * This function also disables all channels and clears the channel groups.
 *
 * @retval     NRF_SUCCESS             If the module was successfully uninitialized.
 * @retval     NRF_ERROR_INVALID_STATE If the module has not been initialized yet.
 * @retval     NRF_ERROR_INTERNAL      If the channels or groups could not be disabled.
 */
uint32_t nrf_drv_ppi_uninit(void);

/**@brief Function for allocating a PPI channel.
 * @details This function allocates the first unused PPI channel.
 *
 * @param[out] p_channel               Pointer to the PPI channel that has been allocated.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully allocated.
 * @retval     NRF_ERROR_NO_MEM        If there is no available channel to be used.
 */
uint32_t nrf_drv_ppi_channel_alloc(nrf_ppi_channel_t * p_channel);

/**@brief Function for freeing a PPI channel.
 * @details This function also disables the chosen channel.
 *
 * @param[in]  channel                 PPI channel to be freed.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully freed.
 * @retval     NRF_ERROR_INVALID_PARAM If the channel is not user-configurable.
 */
uint32_t nrf_drv_ppi_channel_free(nrf_ppi_channel_t channel);

/**@brief Function for assigning task and event endpoints to the PPI channel.
 *
 * @param[in]  channel                 PPI channel to be assigned endpoints.
 *
 * @param[in]  eep                     Event endpoint address.
 *
 * @param[in]  tep                     Task endpoint address.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully assigned.
 * @retval     NRF_ERROR_INVALID_STATE If the channel is not allocated for the user.
 * @retval     NRF_ERROR_INVALID_PARAM If the channel is not user-configurable.
 */
uint32_t nrf_drv_ppi_channel_assign(nrf_ppi_channel_t channel, uint32_t eep, uint32_t tep);

/**@brief Function for enabling a PPI channel.
 *
 * @param[in]  channel                 PPI channel to be enabled.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully enabled.
 * @retval     NRF_ERROR_INVALID_STATE If the user-configurable channel is not allocated.
 * @retval     NRF_ERROR_INVALID_PARAM If the channel cannot be enabled by the user.
 */
uint32_t nrf_drv_ppi_channel_enable(nrf_ppi_channel_t channel);

/**@brief Function for disabling a PPI channel.
 *
 * @param[in]  channel                 PPI channel to be disabled.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully disabled.
 * @retval     NRF_ERROR_INVALID_STATE If the user-configurable channel is not allocated.
 * @retval     NRF_ERROR_INVALID_PARAM If the channel cannot be disabled by the user.
 */
uint32_t nrf_drv_ppi_channel_disable(nrf_ppi_channel_t channel);

/**@brief Function for allocating a PPI channel group.
 * @details This function allocates the first unused PPI group.
 *
 * @param[out] p_group                 Pointer to the PPI channel group that has been allocated.
 *
 * @retval     NRF_SUCCESS             If the channel group was successfully allocated.
 * @retval     NRF_ERROR_NO_MEM        If there is no available channel group to be used.
 */
uint32_t nrf_drv_ppi_group_alloc(nrf_ppi_channel_group_t * p_group);

/**@brief Function for freeing a PPI channel group.
 * @details This function also disables the chosen group.
 *
 * @param[in]  group                   PPI channel group to be freed.
 *
 * @retval     NRF_SUCCESS             If the channel group was successfully freed.
 * @retval     NRF_ERROR_INVALID_PARAM If the channel group is not user-configurable.
 */
uint32_t nrf_drv_ppi_group_free(nrf_ppi_channel_group_t group);

/**@brief Function for including a PPI channel in a channel group.
 *
 * @param[in]  channel                 PPI channel to be added.
 *
 * @param[in]  group                   Channel group in which to include the channel.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully included.
 */
uint32_t nrf_drv_ppi_channel_include_in_group(nrf_ppi_channel_t       channel,
                                              nrf_ppi_channel_group_t group);

/**@brief Function for removing a PPI channel from a channel group.
 *
 * @param[in]  channel                 PPI channel to be removed.
 *
 * @param[in]  group                   Channel group from which to remove the channel.
 *
 * @retval     NRF_SUCCESS             If the channel was successfully removed.
 */
uint32_t nrf_drv_ppi_channel_remove_from_group(nrf_ppi_channel_t       channel,
                                               nrf_ppi_channel_group_t group);

/**@brief Function for enabling a PPI channel group.
 *
 * @param[in]  group                   Channel group to be enabled.
 *
 * @retval     NRF_SUCCESS             If the group was successfully enabled.
 */
uint32_t nrf_drv_ppi_group_enable(nrf_ppi_channel_group_t group);

/**@brief Function for disabling a PPI channel group.
 *
 * @param[in]  group                   Channel group to be disabled.
 *
 * @retval     NRF_SUCCESS             If the group was successfully disabled.
 */
uint32_t nrf_drv_ppi_group_disable(nrf_ppi_channel_group_t group);

/**
 * @brief Function for getting the address of a PPI task.
 *
 * @param[in]  task                      Task.
 * @param[out] p_task                    Pointer to the task address.
 *
 * @retval     NRF_SUCCESS               If the address was successfully copied.
 */
uint32_t nrf_drv_ppi_task_addr_get(nrf_ppi_tasks_t task, uint32_t * p_task);

/**
 *@}
 **/

/*lint --flb "Leave library region" */
#endif // NRF_DRV_PPI_H
