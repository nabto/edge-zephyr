/* modem.c - Copyright Svep Design Center AB */

/**
 * @file modem.c
 *
 * This module handles the modem. It initializes the modem and connects to the LTE network.
*/

/**** Includes ****/
#include "modem.h"

#include <stdio.h>
#include <stdbool.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_info.h>
#include <modem/modem_key_mgmt.h>
//#include <net/nrf_cloud.h>


/**** Defines ****/


/**** Types ****/


/**** Data ****/
LOG_MODULE_REGISTER(modem, LOG_LEVEL_DBG);

//static int modem_lib_init_result = -1;
static modem_event_cb_t m_cb;
K_SEM_DEFINE(lte_connected, 0, 1);

/**** Private functions ****/
static void lte_handler(const struct lte_lc_evt *const evt)
{
    LOG_INF("Callback : %d \n", evt->type);

    switch (evt->type) {
    case LTE_LC_EVT_NW_REG_STATUS:
        if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
             (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
            break;
        }

        LOG_DBG("Network registration status: %s\n",
            evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
            "Connected - home network" : "Connected - roaming");

        //m_cb(MODEM_EVENT_CONNECTED, 0);

        break;

    case LTE_LC_EVT_PSM_UPDATE:
        LOG_DBG("PSM parameter update: TAU: %d, Active time: %d\n",
            evt->psm_cfg.tau, evt->psm_cfg.active_time);
        break;

    case LTE_LC_EVT_EDRX_UPDATE: {
        char log_buf[60];
        int len;

        len = snprintf(log_buf, sizeof(log_buf),
                   "eDRX parameter update: eDRX: %f, PTW: %f",
                   evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
        if (len > 0) {
            LOG_DBG("%s\n", log_buf);
        }
        break;
    }

    case LTE_LC_EVT_RRC_UPDATE:
        LOG_DBG("RRC mode: %s\n",
            evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
            "Connected" : "Idle");
        //m_cb(MODEM_EVENT_CONNECTED, 1);
        break;

    case LTE_LC_EVT_CELL_UPDATE:


        LOG_DBG("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
            evt->cell.id, evt->cell.tac);
        break;

    default:
        break;
    }
}


/**** Public functions ****/
/**
 * @brief Initializes the modem module.
 *
 * @param[in] cb The callback function to call when a modem event occurs.
 *
 * @return int Zero on success or (negative) error code otherwise.
*/
int modem_init(modem_event_cb_t cb)
{
    m_cb = cb;
    LOG_INF("Initializing modem library");

	int ret = nrf_modem_lib_init();

	/* Handle return values relating to modem firmware update */

	switch (ret) {
	case 0:
		/* Initialization successful, no action required. */
        LOG_INF("Modem library initialized");
		return 0;
	//case NRF_MODEM_LIB_H:
	//	LOG_DBG("MODEM UPDATE OK. Will run new modem firmware after reboot");
	//	break;
	case NRF_MODEM_DFU_RESULT_UUID_ERROR:
	case NRF_MODEM_DFU_RESULT_AUTH_ERROR:
		LOG_ERR("MODEM UPDATE ERROR %d. Will run old firmware", ret);
		break;
	case NRF_MODEM_DFU_RESULT_HARDWARE_ERROR:
	case NRF_MODEM_DFU_RESULT_INTERNAL_ERROR:
		LOG_ERR("MODEM UPDATE FATAL ERROR %d. Modem failure", ret);
		break;
	//case MODEM_DFU_RESULT_VOLTAGE_LOW:
	//	LOG_ERR("MODEM UPDATE CANCELLED %d.", ret);
	//	LOG_ERR("Please reboot once you have sufficient power for the DFU");
	//	break;
	default:
		/* All non-zero return codes other than DFU result codes are
		 * considered irrecoverable and a reboot is needed.
		 */
		LOG_ERR("nRF modem lib initialization failed, error: %d", ret);
		break;
	}

    ret = lte_lc_init();
// prepeare for fota
//#if defined(CONFIG_NRF_CLOUD_FOTA)
//	/* Ignore return value, rebooting below */
//	(void)nrf_cloud_fota_pending_job_validate(NULL);
//#endif
//	LOG_DBG("Rebooting...");
//	LOG_PANIC();
//	sys_reboot(SYS_REBOOT_COLD);
    return ret;
}


/**
 * @brief Connects to the LTE network.
 *
 * @return int Zero on success or (negative) error code otherwise.
*/
int modem_connect(void)
{
    int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */

	} else {
		err = lte_lc_init_and_connect_async(lte_handler);;
		if (err) {
			LOG_ERR("Modem could not be configured, error: %d\n",
				err);
			return err;
		}
	}

    m_cb(MODEM_EVENT_CONNECTING, 0);

    return 0;
}
