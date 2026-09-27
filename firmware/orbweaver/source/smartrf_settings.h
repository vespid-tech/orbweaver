/*
 * Orbweaver - smartrf_settings.h
 * Copyright (C) 2026 Vespid
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _SMARTRF_SETTINGS_H_
#define _SMARTRF_SETTINGS_H_

#include "./driverlib/rf_prop_cmd.h"
#include "./driverlib/rf_common_cmd.h"
#include "./inc/hw_rfc_dbell.h"
#include "./driverlib/rf_mailbox.h"

#include <C:/ti/simplelink_cc13x0_sdk_4_20_02_07/source/ti/drivers/rf/RF.h>


extern RF_Mode RF_prop_kbps_50;
extern RF_Mode RF_prop_bps_625;
extern RF_Mode RF_prop_kbps_2p5_5;

extern uint32_t pOverrides_bps_625[];
extern uint32_t pOverrides_kbps_2p5[];
extern uint32_t pOverrides_kbps_5[];
extern uint32_t pOverrides_kbps_50[];
extern uint32_t pOverrides_kbps_200[];
extern uint32_t pOverrides_kbps_300_400[];
extern uint32_t pOverrides_kbps_500[];


// RF Core API Commands
extern rfc_CMD_PROP_RADIO_DIV_SETUP_t RF_cmdPropRadioDivSetup;
extern rfc_CMD_FS_t RF_cmdFs;
extern rfc_CMD_PROP_TX_ADV_t RF_cmdPropTxAdv;
extern rfc_CMD_PROP_RX_ADV_t RF_cmdPropRxAdv;

// RF Core API Overrides
extern uint32_t pOverrides[];

#endif // _SMARTRF_SETTINGS_H_
