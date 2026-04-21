#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>

// Build-time feature defaults for progressive bring-up.
#ifndef ENABLE_UWB
#define ENABLE_UWB 1
#endif

#ifndef ENABLE_ESKF
#define ENABLE_ESKF 1
#endif

#ifndef ENABLE_MIC_ADC
#define ENABLE_MIC_ADC 0
#endif

// Bring-up stages:
//   0 = IMU only
//   1 = UWB detect only
//   2 = single-UWB ranging
//   3 = full T/L/R ranging
//   4 = full fusion (default)
#define BRINGUP_STAGE_IMU_ONLY      0
#define BRINGUP_STAGE_UWB_DETECT    1
#define BRINGUP_STAGE_SINGLE_RANGE  2
#define BRINGUP_STAGE_FULL_RANGE    3
#define BRINGUP_STAGE_FULL_FUSION   4

#ifndef BRINGUP_STAGE
#define BRINGUP_STAGE BRINGUP_STAGE_FULL_FUSION
#endif

#define BRINGUP_HAS_UWB_INIT        (ENABLE_UWB && (BRINGUP_STAGE >= BRINGUP_STAGE_UWB_DETECT))
#define BRINGUP_HAS_UWB_RANGING     (ENABLE_UWB && (BRINGUP_STAGE >= BRINGUP_STAGE_SINGLE_RANGE))
#define BRINGUP_HAS_FULL_RANGE      (BRINGUP_STAGE >= BRINGUP_STAGE_FULL_RANGE)
#define BRINGUP_HAS_FILTER          (ENABLE_ESKF && (BRINGUP_STAGE >= BRINGUP_STAGE_FULL_FUSION))

// Main loop rates tuned for the production Teensy board.
#define IMU_LOOP_PERIOD_MS      5U      // 200 Hz
#define UWB_CYCLE_PERIOD_MS     200U    // 5 Hz at fixed 2 MHz SPI1
#define TELEMETRY_PERIOD_MS     50U     // 20 Hz
#define CALIBRATION_SAMPLES     500U
#define FILTER_MAX_PREDICT_DT_S 0.050f

// Timer-driven ADC scan for direct analog microphone bring-up.
// IMPORTANT: the active Sheet1.NET mainboard routes the tool mic to IC1-17 and the
// left/right satellite mic lines through U30/U31 analog stages instead of direct A0..A4.
// Leave this disabled by default until the integrated audio path is mapped in firmware.
#define MIC_ADC_RESOLUTION_BITS 12U
#define MIC_ADC_AVERAGING       1U
#define MIC_SCAN_PERIOD_US      20U
#define MIC_BLOCK_FRAMES        64U

// In single-range bring-up, choose which headset UWB initiates toward the tool.
#define UWB_SINGLE_INITIATOR_IDX 0U

// Teensy 4.1 IMU wiring (SPI).
// Active board source of truth: ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md
// Logical pins are used in firmware; comments include the Altium symbol names.
#define IMU_PIN_CS              10
#define IMU_PIN_MOSI            11
#define IMU_PIN_MISO            12
#define IMU_PIN_SCK             13

// Fixed rotation R_HI from IMU frame {I} to headset body frame {H}.
// Defaults to identity until the production board mounting is measured.
#define IMU_R_HI_00             1.0f
#define IMU_R_HI_01             0.0f
#define IMU_R_HI_02             0.0f
#define IMU_R_HI_10             0.0f
#define IMU_R_HI_11             1.0f
#define IMU_R_HI_12             0.0f
#define IMU_R_HI_20             0.0f
#define IMU_R_HI_21             0.0f
#define IMU_R_HI_22             1.0f

// Teensy 4.1 UWB shared SPI1 bus wiring.
#define UWB_PIN_MISO            1
#define UWB_PIN_MOSI            26
#define UWB_PIN_SCK             27
#define UWB_SPI_BAUD            2000000UL
#define UWB_MAX_RETRIES         2U

// Baseline DW3000 PHY profile for the production headset board.
// These values match the current design docs and are applied in dwm3000_configure_default().
#define UWB_CHANNEL             5U
#define UWB_SFD_TYPE            0U      // IEEE 802.15.4 short 8-symbol SFD
#define UWB_PREAMBLE_CODE       9U      // 64 MHz PRF
#define UWB_PREAMBLE_LENGTH     128U

// Per-module antenna-delay placeholders.
// Replace with measured values after on-bench calibration.
#define UWB_T_TX_ANT_DLY        16385U
#define UWB_T_RX_ANT_DLY        16385U
#define UWB_L_TX_ANT_DLY        16385U
#define UWB_L_RX_ANT_DLY        16385U
#define UWB_R_TX_ANT_DLY        16385U
#define UWB_R_RX_ANT_DLY        16385U
#define UWB_TOOL_TX_ANT_DLY     16385U
#define UWB_TOOL_RX_ANT_DLY     16385U

// Per-module UWB control lines.
// Top UWB: D0/D2/D3 -> Altium RX1/OUT2/LRCLK2.
#define UWB_T_CS                0
#define UWB_T_RST               2
#define UWB_T_IRQ               3

// Left UWB: D4/D29/D6 -> Altium BCLK2/TX7/OUT1D.
#define UWB_L_CS                4
#define UWB_L_RST               29
#define UWB_L_IRQ               6

// Right UWB: D36/D37/D30 -> Altium CS_2/CS_3/CRX3.
#define UWB_R_CS                36
#define UWB_R_RST               37
#define UWB_R_IRQ               30

// Tool UWB: D24/D25/D28 -> Altium A10/A11/RX7.
#define UWB_TOOL_CS             24
#define UWB_TOOL_RST            25
#define UWB_TOOL_IRQ            28

// Direct-ADC microphone placeholders kept only for bench experiments on older/simple builds.
// These are not the active Sheet1.NET integrated-board signal paths.
#define MIC_TOOL_PIN            A0
#define MIC_OUT_L_PIN           A1
#define MIC_OUT_R_PIN           A2
#define MIC_IN_L_PIN            A3
#define MIC_IN_R_PIN            A4
#define MIC_CHANNEL_COUNT       5

// Headset body-frame UWB locations (metres).
#define UWB_T_BX                0.00f
#define UWB_T_BY                0.00f
#define UWB_T_BZ                0.10f

#define UWB_L_BX               -0.08f
#define UWB_L_BY                0.00f
#define UWB_L_BZ                0.00f

#define UWB_R_BX                0.08f
#define UWB_R_BY                0.00f
#define UWB_R_BZ                0.00f

// Tool world-frame position placeholder; measure on deployment.
#define TOOL_WX                 0.0f
#define TOOL_WY                 1.5f
#define TOOL_WZ                 0.8f

#endif // BOARD_CONFIG_H
