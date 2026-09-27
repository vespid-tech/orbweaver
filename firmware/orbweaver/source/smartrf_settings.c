/*
 * Orbweaver - smartrf_settings.c
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

#include "smartrf_settings.h"

#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_genfsk.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_rfe_genfsk.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_lrm.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_rfe_lrm.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_brepeat.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_rfe_brepeat.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_mce_brepeat.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_cpe_sl_longrange.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_rfe_sl_longrange.h)
#include DeviceFamily_constructPath(rf_patches/rf_patch_mce_sl_longrange.h)



// TI-RTOS RF Mode Object
RF_Mode RF_prop_kbps_50 =
{
    .rfMode = RF_MODE_PROPRIETARY_SUB_1,
    .cpePatchFxn = &rf_patch_cpe_genfsk,
    .mcePatchFxn = 0,
    .rfePatchFxn = &rf_patch_rfe_genfsk
};

RF_Mode RF_prop_bps_625 =
{
    .rfMode = RF_MODE_PROPRIETARY_SUB_1,
    .cpePatchFxn = &rf_patch_cpe_lrm,
    .mcePatchFxn = 0,
    .rfePatchFxn = &rf_patch_rfe_lrm
};


RF_Mode RF_prop_kbps_2p5_5 =
{
    .rfMode = RF_MODE_PROPRIETARY_SUB_1,
    .cpePatchFxn = &rf_patch_cpe_sl_longrange,
    .mcePatchFxn = &rf_patch_mce_sl_longrange,
    .rfePatchFxn = &rf_patch_rfe_sl_longrange
};

uint32_t pOverrides_bps_625[] =
{
    // override_use_patch_prop_lrm.xml
    // PHY: Use MCE ROM bank 3, RFE RAM patch
    MCE_RFE_OVERRIDE(0,3,0,1,0,0),
    // override_synth_prop_863_930_div5.xml
    // Synth: Set recommended RTRIM to 7
    HW_REG_OVERRIDE(0x4038,0x0037),
    // Synth: Set Fref to 4 MHz
    (uint32_t)0x000684A3,
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4020,0x7F00),
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4064,0x0040),
    // Synth: Configure fine calibration setting
    (uint32_t)0xB1070503,
    // Synth: Configure fine calibration setting
    (uint32_t)0x05330523,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x0A480583,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x7AB80603,
    // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
    ADI_REG_OVERRIDE(1,4,0x9F),
    // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
    ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
    // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
    (uint32_t)0x02010403,
    // Synth: Configure extra PLL filtering
    (uint32_t)0x00108463,
    // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
    (uint32_t)0x04B00243,
    // override_phy_rx_aaf_bw_0xd.xml
    // Rx: Set anti-aliasing filter bandwidth to 0xD (in ADI0, set IFAMPCTL3[7:4]=0xD)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0xD),
    // override_phy_gfsk_rx.xml
    // Rx: Set LNA bias current trim offset to 3
    (uint32_t)0x00038883,
    // Rx: Freeze RSSI on sync found event
    HW_REG_OVERRIDE(0x6084,0x35F1),
    // override_phy_gfsk_pa_ramp_agc_reflevel_0x1a.xml
    // Tx: Configure PA ramping setting (0x41). Rx: Set AGC reference level to 0x1A.
    HW_REG_OVERRIDE(0x6088,0x411A),
    // Tx: Configure PA ramping setting
    HW_REG_OVERRIDE(0x608C,0x8213),
    // override_phy_lrm_rom_dsss8.xml
    // PHY: Configure DSSS=8
    HW_REG_OVERRIDE(0x505C,0x073C),
    // override_phy_rx_rssi_offset_5db.xml
    // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
    (uint32_t)0x00FB88A3,
    // TX power override
    // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
    ADI_REG_OVERRIDE(0,12,0xF8),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_5[] =
{
     // override_use_patch_simplelink_long_range.xml
     // PHY: Use MCE RAM patch, RFE RAM patch
     MCE_RFE_OVERRIDE(1,0,0,1,0,0),
     // override_synth_prop_863_930_div5_lbw60k.xml
     // Synth: Set recommended RTRIM to 7
     HW_REG_OVERRIDE(0x4038,0x0037),
     // Synth: Set Fref to 4 MHz
     (uint32_t)0x000684A3,
     // Synth: Configure fine calibration setting
     HW_REG_OVERRIDE(0x4020,0x7F00),
     // Synth: Configure fine calibration setting
     HW_REG_OVERRIDE(0x4064,0x0040),
     // Synth: Configure fine calibration setting
     (uint32_t)0xB1070503,
     // Synth: Configure fine calibration setting
     (uint32_t)0x05330523,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x40410583,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x32CC0603,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x00010623,
     // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
     ADI_REG_OVERRIDE(1,4,0x9F),
     // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
     ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
     // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
     (uint32_t)0x02010403,
     // Synth: Configure extra PLL filtering
     (uint32_t)0x00108463,
     // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
     (uint32_t)0x04B00243,
     // override_synth_disable_bias_div5.xml
     // Synth: Set divider bias to disabled
     HW32_ARRAY_OVERRIDE(0x405C,1),
     // Synth: Set divider bias to disabled (specific for loDivider=5)
     (uint32_t)0x18000200,
     // override_phy_rx_aaf_bw_0xd.xml
     // Rx: Set anti-aliasing filter bandwidth to 0xD (in ADI0, set IFAMPCTL3[7:4]=0xD)
     ADI_HALFREG_OVERRIDE(0,61,0xF,0xD),
     // override_phy_gfsk_rx.xml
     // Rx: Set LNA bias current trim offset to 3
     (uint32_t)0x00038883,
     // Rx: Freeze RSSI on sync found event
     HW_REG_OVERRIDE(0x6084,0x35F1),
     // override_phy_gfsk_pa_ramp_agc_reflevel_0x14.xml
     // Tx: Configure PA ramping setting (0x41). Rx: Set AGC reference level to 0x14.
     HW_REG_OVERRIDE(0x6088,0x4114),
     // Tx: Configure PA ramping setting
     HW_REG_OVERRIDE(0x608C,0x8213),
     // override_phy_long_range_dsss2.xml
     // PHY: Configure DSSS SF=2
     HW_REG_OVERRIDE(0x505C,0x0100),
     // override_phy_rx_rssi_offset_5db.xml
     // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
     (uint32_t)0x00FB88A3,
     // TX power override
     // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
     ADI_REG_OVERRIDE(0,12,0xF8),
     (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_2p5[] =
{
     // override_use_patch_simplelink_long_range.xml
     // PHY: Use MCE RAM patch, RFE RAM patch
     MCE_RFE_OVERRIDE(1,0,0,1,0,0),
     // override_synth_prop_863_930_div5_lbw60k.xml
     // Synth: Set recommended RTRIM to 7
     HW_REG_OVERRIDE(0x4038,0x0037),
     // Synth: Set Fref to 4 MHz
     (uint32_t)0x000684A3,
     // Synth: Configure fine calibration setting
     HW_REG_OVERRIDE(0x4020,0x7F00),
     // Synth: Configure fine calibration setting
     HW_REG_OVERRIDE(0x4064,0x0040),
     // Synth: Configure fine calibration setting
     (uint32_t)0xB1070503,
     // Synth: Configure fine calibration setting
     (uint32_t)0x05330523,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x40410583,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x32CC0603,
     // Synth: Set loop bandwidth after lock to 60 kHz
     (uint32_t)0x00010623,
     // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
     ADI_REG_OVERRIDE(1,4,0x9F),
     // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
     ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
     // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
     (uint32_t)0x02010403,
     // Synth: Configure extra PLL filtering
     (uint32_t)0x00108463,
     // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
     (uint32_t)0x04B00243,
     // override_synth_disable_bias_div5.xml
     // Synth: Set divider bias to disabled
     HW32_ARRAY_OVERRIDE(0x405C,1),
     // Synth: Set divider bias to disabled (specific for loDivider=5)
     (uint32_t)0x18000200,
     // override_phy_rx_aaf_bw_0xd.xml
     // Rx: Set anti-aliasing filter bandwidth to 0xD (in ADI0, set IFAMPCTL3[7:4]=0xD)
     ADI_HALFREG_OVERRIDE(0,61,0xF,0xD),
     // override_phy_gfsk_rx.xml
     // Rx: Set LNA bias current trim offset to 3
     (uint32_t)0x00038883,
     // Rx: Freeze RSSI on sync found event
     HW_REG_OVERRIDE(0x6084,0x35F1),
     // override_phy_gfsk_pa_ramp_agc_reflevel_0x14.xml
     // Tx: Configure PA ramping setting (0x41). Rx: Set AGC reference level to 0x14.
     HW_REG_OVERRIDE(0x6088,0x4114),
     // Tx: Configure PA ramping setting
     HW_REG_OVERRIDE(0x608C,0x8213),
     // override_phy_long_range_dsss4.xml
     // PHY: Configure DSSS SF=4
     HW_REG_OVERRIDE(0x505C,0x0303),
     // override_phy_rx_rssi_offset_5db.xml
     // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
     (uint32_t)0x00FB88A3,
     // TX power override
     // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
     ADI_REG_OVERRIDE(0,12,0xF8),
     (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_50[] =
{
    // override_use_patch_prop_genfsk.xml
    // PHY: Use MCE ROM bank 4, RFE RAM patch
    MCE_RFE_OVERRIDE(0,4,0,1,0,0),
    // override_synth_prop_863_930_div5.xml
    // Synth: Set recommended RTRIM to 7
    HW_REG_OVERRIDE(0x4038,0x0037),
    // Synth: Set Fref to 4 MHz
    (uint32_t)0x000684A3,
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4020,0x7F00),
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4064,0x0040),
    // Synth: Configure fine calibration setting
    (uint32_t)0xB1070503,
    // Synth: Configure fine calibration setting
    (uint32_t)0x05330523,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x0A480583,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x7AB80603,
    // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
    ADI_REG_OVERRIDE(1,4,0x9F),
    // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
    ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
    // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
    (uint32_t)0x02010403,
    // Synth: Configure extra PLL filtering
    (uint32_t)0x00108463,
    // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
    (uint32_t)0x04B00243,
    // override_phy_rx_aaf_bw_0xd.xml
    // Rx: Set anti-aliasing filter bandwidth to 0xD (in ADI0, set IFAMPCTL3[7:4]=0xD)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0xD),
    // override_phy_gfsk_rx.xml
    // Rx: Set LNA bias current trim offset to 3
    (uint32_t)0x00038883,
    // Rx: Freeze RSSI on sync found event
    HW_REG_OVERRIDE(0x6084,0x35F1),
    // override_phy_gfsk_pa_ramp_agc_reflevel_0x1a.xml
    // Tx: Configure PA ramping setting (0x41). Rx: Set AGC reference level to 0x1A.
    HW_REG_OVERRIDE(0x6088,0x411A),
    // Tx: Configure PA ramping setting
    HW_REG_OVERRIDE(0x608C,0x8213),
    // override_phy_rx_rssi_offset_5db.xml
    // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
    (uint32_t)0x00FB88A3,
    // TX power override
    // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
    ADI_REG_OVERRIDE(0,12,0xF8),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_200[] =
{
    // override_use_patch_prop_genfsk.xml
    // PHY: Use MCE ROM bank 4, RFE RAM patch
    MCE_RFE_OVERRIDE(0,4,0,1,0,0),
    // override_synth_prop_863_930_div5.xml
    // Synth: Set recommended RTRIM to 7
    HW_REG_OVERRIDE(0x4038,0x0037),
    // Synth: Set Fref to 4 MHz
    (uint32_t)0x000684A3,
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4020,0x7F00),
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4064,0x0040),
    // Synth: Configure fine calibration setting
    (uint32_t)0xB1070503,
    // Synth: Configure fine calibration setting
    (uint32_t)0x05330523,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x0A480583,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x7AB80603,
    // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
    ADI_REG_OVERRIDE(1,4,0x9F),
    // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
    ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
    // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
    (uint32_t)0x02010403,
    // Synth: Configure extra PLL filtering
    (uint32_t)0x00108463,
    // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
    (uint32_t)0x04B00243,
    // override_synth_disable_bias_div5.xml
    // Synth: Set divider bias to disabled
    HW32_ARRAY_OVERRIDE(0x405C,1),
    // Synth: Set divider bias to disabled (specific for loDivider=5)
    (uint32_t)0x18000200,
    // override_phy_rx_aaf_bw_0x0.xml
    // Rx: Set anti-aliasing filter bandwidth to 0x0 (in ADI0, set IFAMPCTL3[7:4]=0x0)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0x0),
    // override_phy_gfsk_rx.xml
    // Rx: Set LNA bias current trim offset to 3
    (uint32_t)0x00038883,
    // Rx: Freeze RSSI on sync found event
    HW_REG_OVERRIDE(0x6084,0x35F1),
    // override_phy_gfsk_pa_ramp_5us_agc_reflevel_0x1c.xml
    // Tx: Configure PA ramping setting (0x10) for approximately 5 us PA ramp time. Rx: Set AGC reference level to 0x1C.
    HW_REG_OVERRIDE(0x6088,0x101C),
    // Tx: Configure PA ramping setting (0x08) for approximately 5 us PA ramp time
    HW_REG_OVERRIDE(0x608C,0x0813),
    // override_phy_rx_rssi_offset_5db.xml
    // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
    (uint32_t)0x00FB88A3,
    // TX power override
    // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
    ADI_REG_OVERRIDE(0,12,0xF8),
    (uint32_t)0xFFFFFFFF
};

// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_300_400[] =
{
    // override_use_patch_prop_genfsk.xml
    // PHY: Use MCE ROM bank 4, RFE RAM patch
    MCE_RFE_OVERRIDE(0,4,0,1,0,0),
    // override_synth_prop_863_930_div5.xml
    // Synth: Set recommended RTRIM to 7
    HW_REG_OVERRIDE(0x4038,0x0037),
    // Synth: Set Fref to 4 MHz
    (uint32_t)0x000684A3,
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4020,0x7F00),
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4064,0x0040),
    // Synth: Configure fine calibration setting
    (uint32_t)0xB1070503,
    // Synth: Configure fine calibration setting
    (uint32_t)0x05330523,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x0A480583,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x7AB80603,
    // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
    ADI_REG_OVERRIDE(1,4,0x9F),
    // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
    ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
    // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
    (uint32_t)0x02010403,
    // Synth: Configure extra PLL filtering
    (uint32_t)0x00108463,
    // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
    (uint32_t)0x04B00243,
    // override_synth_disable_bias_div5.xml
    // Synth: Set divider bias to disabled
    HW32_ARRAY_OVERRIDE(0x405C,1),
    // Synth: Set divider bias to disabled (specific for loDivider=5)
    (uint32_t)0x18000200,
    // override_phy_rx_aaf_bw_0x0.xml
    // Rx: Set anti-aliasing filter bandwidth to 0x0 (in ADI0, set IFAMPCTL3[7:4]=0x0)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0x0),
    // override_phy_gfsk_rx.xml
    // Rx: Set LNA bias current trim offset to 3
    (uint32_t)0x00038883,
    // Rx: Freeze RSSI on sync found event
    HW_REG_OVERRIDE(0x6084,0x35F1),
    // override_phy_gfsk_pa_ramp_5us_agc_reflevel_0x1e.xml
    // Tx: Configure PA ramping setting (0x10) for approximately 5 us PA ramp time. Rx: Set AGC reference level to 0x1E.
    HW_REG_OVERRIDE(0x6088,0x101E),
    // Tx: Configure PA ramping setting (0x08) for approximately 5 us PA ramp time
    HW_REG_OVERRIDE(0x608C,0x0813),
    // override_phy_rx_rssi_offset_5db.xml
    // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
    (uint32_t)0x00FB88A3,
    // TX power override
    // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
    ADI_REG_OVERRIDE(0,12,0xF8),
    (uint32_t)0xFFFFFFFF
};


// Overrides for CMD_PROP_RADIO_DIV_SETUP
uint32_t pOverrides_kbps_500[] =
{
    // override_use_patch_prop_genfsk.xml
    // PHY: Use MCE ROM bank 4, RFE RAM patch
    MCE_RFE_OVERRIDE(0,4,0,1,0,0),
    // override_synth_prop_863_930_div5.xml
    // Synth: Set recommended RTRIM to 7
    HW_REG_OVERRIDE(0x4038,0x0037),
    // Synth: Set Fref to 4 MHz
    (uint32_t)0x000684A3,
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4020,0x7F00),
    // Synth: Configure fine calibration setting
    HW_REG_OVERRIDE(0x4064,0x0040),
    // Synth: Configure fine calibration setting
    (uint32_t)0xB1070503,
    // Synth: Configure fine calibration setting
    (uint32_t)0x05330523,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x0A480583,
    // Synth: Set loop bandwidth after lock to 20 kHz
    (uint32_t)0x7AB80603,
    // Synth: Configure VCO LDO (in ADI1, set VCOLDOCFG=0x9F to use voltage input reference)
    ADI_REG_OVERRIDE(1,4,0x9F),
    // Synth: Configure synth LDO (in ADI1, set SLDOCTL0.COMP_CAP=1)
    ADI_HALFREG_OVERRIDE(1,7,0x4,0x4),
    // Synth: Use 24 MHz XOSC as synth clock, enable extra PLL filtering
    (uint32_t)0x02010403,
    // Synth: Configure extra PLL filtering
    (uint32_t)0x00108463,
    // Synth: Increase synth programming timeout (0x04B0 RAT ticks = 300 us)
    (uint32_t)0x04B00243,
    // override_synth_disable_bias_div5.xml
    // Synth: Set divider bias to disabled
    HW32_ARRAY_OVERRIDE(0x405C,1),
    // Synth: Set divider bias to disabled (specific for loDivider=5)
    (uint32_t)0x18000200,
    // override_phy_rx_aaf_bw_0x0.xml
    // Rx: Set anti-aliasing filter bandwidth to 0x0 (in ADI0, set IFAMPCTL3[7:4]=0x0)
    ADI_HALFREG_OVERRIDE(0,61,0xF,0x0),
    // override_phy_gfsk_rx.xml
    // Rx: Set LNA bias current trim offset to 3
    (uint32_t)0x00038883,
    // Rx: Freeze RSSI on sync found event
    HW_REG_OVERRIDE(0x6084,0x35F1),
    // override_phy_gfsk_pa_ramp_5us_agc_reflevel_0x24.xml
    // Tx: Configure PA ramping setting (0x10) for approximately 5 us PA ramp time. Rx: Set AGC reference level to 0x24.
    HW_REG_OVERRIDE(0x6088,0x1024),
    // Tx: Configure PA ramping setting (0x08) for approximately 5 us PA ramp time
    HW_REG_OVERRIDE(0x608C,0x0813),
    // override_phy_rx_rssi_offset_5db.xml
    // Rx: Set RSSI offset to adjust reported RSSI by +5 dB (default: 0), trimmed for external bias and differential configuration
    (uint32_t)0x00FB88A3,
    // TX power override
    // Tx: Set PA trim to max (in ADI0, set PACTL0=0xF8)
    ADI_REG_OVERRIDE(0,12,0xF8),
    (uint32_t)0xFFFFFFFF
};
// CMD_PROP_RADIO_DIV_SETUP
// Proprietary Mode Radio Setup Command for All Frequency Bands
rfc_CMD_PROP_RADIO_DIV_SETUP_t RF_cmdPropRadioDivSetup =
{
    .commandNo = 0x3807,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .modulation.modType = 0x1,
    .modulation.deviation = 0x64,
    .symbolRate.preScale = 0xF,
    .symbolRate.rateWord = 0x8000,  //symbol rate = (rateWord × 24,000,000) / (preScale × 2^20). So preScale = 0xF and rateWord = 0x8000 is symbolRate = 50,000
    .symbolRate.decimMode = 0x0,
    .rxBw = 0x24,
    .preamConf.nPreamBytes = 0x4,
    .preamConf.preamMode = 0x0,
    .formatConf.nSwBits = 0x20,
    .formatConf.bBitReversal = 0x0,
    .formatConf.bMsbFirst = 0x1,
    .formatConf.fecMode = 0x0,
    .formatConf.whitenMode = 0x1,
    .config.frontEndMode = 0x0,
    .config.biasMode = 0x1,
    .config.analogCfgMode = 0x0,
    .config.bNoFsPowerUp = 0x0,
    .txPower = 0xA73F,
    .pRegOverride = pOverrides_kbps_50,
    .centerFreq = 0x0393,
    .intFreq = 0x8000,
    .loDivider = 0x05
};


// CMD_FS
// Frequency Synthesizer Programming Command
rfc_CMD_FS_t RF_cmdFs =
{
    .commandNo = 0x0803,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .frequency = 0x0393,
    .fractFreq = 0x0000,
    .synthConf.bTxMode = 0x0,
    .synthConf.refFreq = 0x0,
    .__dummy0 = 0x00,
    .__dummy1 = 0x00,
    .__dummy2 = 0x00,
    .__dummy3 = 0x0000
};

/*
// CMD_PROP_TX
// Proprietary Mode Transmit Command
rfc_CMD_PROP_TX_t RF_cmdPropTx =
{
    .commandNo = 0x3801,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bVarLen = 0x1,
    .pktLen = 0x14,
    .syncWord = 0xD365B92C,
    .pPkt = 0 // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
};
*/

// CMD_PROP_TX_ADV
// Proprietary Mode Advanced Transmit Command
rfc_CMD_PROP_TX_ADV_t RF_cmdPropTxAdv =
{
    .commandNo = 0x3803,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bCrcIncSw = 0x0,
    .pktConf.bCrcIncHdr = 0x0,
    .numHdrBits = 0x10,
    .pktLen = 0x0000,
    .startConf.bExtTxTrig = 0x0,
    .startConf.inputMode = 0x0,
    .startConf.source = 0x0,
    .preTrigger.triggerType = 0x0,
    .preTrigger.bEnaCmd = 0x0,
    .preTrigger.triggerNo = 0x0,
    .preTrigger.pastTrig = 0x0,
    .preTime = 0x00000000,
    .syncWord = 0xD365B92C,
    .pPkt = 0 // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
};

/*
// CMD_PROP_RX
// Proprietary Mode Receive Command
rfc_CMD_PROP_RX_t RF_cmdPropRx =
{
    .commandNo = 0x3802,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bRepeatOk = 0x0,
    .pktConf.bRepeatNok = 0x0,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bVarLen = 0x1,
    .pktConf.bChkAddress = 0x0,
    .pktConf.endType = 0x0,
    .pktConf.filterOp = 0x0,
    .rxConf.bAutoFlushIgnored = 0x0,
    .rxConf.bAutoFlushCrcErr = 0x0,
    .rxConf.bIncludeHdr = 0x1,
    .rxConf.bIncludeCrc = 0x0,
    .rxConf.bAppendRssi = 0x0,
    .rxConf.bAppendTimestamp = 0x0,
    .rxConf.bAppendStatus = 0x1,
    .syncWord = 0xD365B92C,
    .maxPktLen = 0xFF,
    .address0 = 0xAA,
    .address1 = 0xBB,
    .endTrigger.triggerType = 0x1,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .pQueue = 0, // INSERT APPLICABLE POINTER: (dataQueue_t*)&xxx
    .pOutput = 0 // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
};
*/

// CMD_PROP_RX_ADV
// Proprietary Mode Advanced Receive Command
rfc_CMD_PROP_RX_ADV_t RF_cmdPropRxAdv =
{
    .commandNo = 0x3804,
    .status = 0x0000,
    .pNextOp = 0, // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .pktConf.bFsOff = 0x0,
    .pktConf.bRepeatOk = 0x0,
    .pktConf.bRepeatNok = 0x0,
    .pktConf.bUseCrc = 0x1,
    .pktConf.bCrcIncSw = 0x0,
    .pktConf.bCrcIncHdr = 0x0,
    .pktConf.endType = 0x0,
    .pktConf.filterOp = 0x0,
    .rxConf.bAutoFlushIgnored = 0x0,
    .rxConf.bAutoFlushCrcErr = 0x0,
    .rxConf.bIncludeHdr = 0x1,
    .rxConf.bIncludeCrc = 0x0,
    .rxConf.bAppendRssi = 0x1,
    .rxConf.bAppendTimestamp = 0x0,
    .rxConf.bAppendStatus = 0x0,
    .syncWord0 = 0xD365B92C,
    .syncWord1 = 0x00000000,
    .maxPktLen = 0x0400,
    .hdrConf.numHdrBits = 0x10,
    .hdrConf.lenPos = 0x00,
    .hdrConf.numLenBits = 0x10,
    .addrConf.addrType = 0x0,
    .addrConf.addrSize = 0x0,
    .addrConf.addrPos = 0x0,
    .addrConf.numAddr = 0x0,
    .lenOffset = 0x00,
    .endTrigger.triggerType = 0x1,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .pAddr = 0,  // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
    .pQueue = 0, // INSERT APPLICABLE POINTER: (dataQueue_t*)&xxx
    .pOutput = 0 // INSERT APPLICABLE POINTER: (uint8_t*)&xxx
};

