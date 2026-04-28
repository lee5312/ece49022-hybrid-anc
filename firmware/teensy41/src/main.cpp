#include <Arduino.h>
#include <SPI.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "dwm3000.h"
#include "eskf.h"
#include "imu.h"
#include "mic_sampler.h"
#include "uwb_network.h"
#include "uwb_ranging.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kGravity = 9.80665f;
constexpr float kDegPerRad = 180.0f / kPi;
constexpr uint32_t kMicRefMv = 3300U;
constexpr uint32_t kMaxPredictStepUs =
    static_cast<uint32_t>(FILTER_MAX_PREDICT_DT_S * 1000000.0f + 0.5f);
constexpr uint8_t kUwbModuleCount = UWB_NUM_HEADSET + 1U;

enum MicIndex : uint8_t {
    MIC_TOOL = 0,
    MIC_OUT_L = 1,
    MIC_OUT_R = 2,
    MIC_IN_L = 3,
    MIC_IN_R = 4,
};

enum UwbModuleIndex : uint8_t {
    UWB_MODULE_T = 0,
    UWB_MODULE_L = 1,
    UWB_MODULE_R = 2,
    UWB_MODULE_TOOL = 3,
};

struct MicSnapshot {
    uint16_t raw[MIC_CHANNEL_COUNT];
    float volts[MIC_CHANNEL_COUNT];
    uint16_t mean[MIC_CHANNEL_COUNT];
    uint16_t peak_to_peak[MIC_CHANNEL_COUNT];
    uint32_t tick;
    uint32_t frame_counter;
    uint32_t block_counter;
    uint32_t block_timestamp_us;
    uint32_t sample_rate_hz;
    bool running;
    bool block_valid;
};

struct SensorState {
    float accel_g[3];
    float gyro_dps[3];
    bool imu_valid;
    uint32_t imu_tick;
    uint32_t imu_timestamp_us;

    float range_m[UWB_NUM_HEADSET];
    bool range_valid[UWB_NUM_HEADSET];
    uint32_t range_timestamp_us[UWB_NUM_HEADSET];
    uint32_t uwb_tick;
    uint32_t uwb_cycle_timestamp_us;
    bool uwb_ready[kUwbModuleCount];

    MicSnapshot mic;
};

struct FusionImuSample {
    float accel_ms2[3];
    float gyro_rads[3];
    uint32_t timestamp_us;
    bool valid;
};

SensorState s_state = {};
#if BRINGUP_HAS_FILTER
FusionImuSample s_last_imu_sample = {};
#endif

#if BRINGUP_HAS_FILTER
eskf_state_t s_eskf;
bool s_filter_ready = false;
uint32_t s_filter_time_us = 0;
#endif

bool s_imu_ready = false;

#if BRINGUP_HAS_UWB_INIT
dwm3000_inst_t s_uwb_t = { (void *)&SPI1, UWB_T_CS, UWB_T_RST, UWB_T_IRQ, "T", false };
dwm3000_inst_t s_uwb_l = { (void *)&SPI1, UWB_L_CS, UWB_L_RST, UWB_L_IRQ, "L", false };
dwm3000_inst_t s_uwb_r = { (void *)&SPI1, UWB_R_CS, UWB_R_RST, UWB_R_IRQ, "R", false };
dwm3000_inst_t s_uwb_tool = { (void *)&SPI1, UWB_TOOL_CS, UWB_TOOL_RST, UWB_TOOL_IRQ, "Tool", false };
#endif

uint32_t s_next_imu_ms = 0;
uint32_t s_next_uwb_ms = 0;
uint32_t s_next_telemetry_ms = 0;

const uint8_t kMicPins[MIC_CHANNEL_COUNT] = {
    MIC_TOOL_PIN,
    MIC_OUT_L_PIN,
    MIC_OUT_R_PIN,
    MIC_IN_L_PIN,
    MIC_IN_R_PIN,
};

const float kImuToBody[9] = {
    IMU_R_HI_00, IMU_R_HI_01, IMU_R_HI_02,
    IMU_R_HI_10, IMU_R_HI_11, IMU_R_HI_12,
    IMU_R_HI_20, IMU_R_HI_21, IMU_R_HI_22,
};

void serial_printf(const char *fmt, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Serial.print(buffer);
}

const char *bringup_stage_name(void)
{
    switch (BRINGUP_STAGE) {
    case BRINGUP_STAGE_IMU_ONLY:
        return "imu-only";
    case BRINGUP_STAGE_UWB_DETECT:
        return "uwb-detect";
    case BRINGUP_STAGE_SINGLE_RANGE:
        return "single-range";
    case BRINGUP_STAGE_FULL_RANGE:
        return "full-range";
    case BRINGUP_STAGE_FULL_FUSION:
        return "full-fusion";
    default:
        return "custom";
    }
}

#if BRINGUP_HAS_UWB_RANGING
uint8_t active_range_mask(void)
{
#if BRINGUP_STAGE == BRINGUP_STAGE_SINGLE_RANGE
    return static_cast<uint8_t>(1u << UWB_SINGLE_INITIATOR_IDX);
#else
    return UWB_RANGE_MASK_ALL;
#endif
}
#endif

void rotate_imu_to_body(const float in[3], float out[3])
{
    out[0] = kImuToBody[0] * in[0] + kImuToBody[1] * in[1] + kImuToBody[2] * in[2];
    out[1] = kImuToBody[3] * in[0] + kImuToBody[4] * in[1] + kImuToBody[5] * in[2];
    out[2] = kImuToBody[6] * in[0] + kImuToBody[7] * in[1] + kImuToBody[8] * in[2];
}

#if BRINGUP_HAS_FILTER
void predict_filter_to(uint32_t target_us)
{
    if (!s_filter_ready || !s_last_imu_sample.valid) {
        return;
    }

    if (s_filter_time_us == 0U) {
        s_filter_time_us = target_us;
        return;
    }

    while (target_us > s_filter_time_us) {
        uint32_t step_us = target_us - s_filter_time_us;
        if (step_us > kMaxPredictStepUs) {
            step_us = kMaxPredictStepUs;
        }

        const float dt_s = static_cast<float>(step_us) * 1.0e-6f;
        eskf_predict(&s_eskf, s_last_imu_sample.accel_ms2, s_last_imu_sample.gyro_rads, dt_s);
        s_filter_time_us += step_us;
    }
}
#endif

#if BRINGUP_HAS_UWB_INIT
bool init_uwb_module(dwm3000_inst_t *inst, uint16_t short_addr,
                     uint16_t tx_ant_dly, uint16_t rx_ant_dly)
{
    serial_printf("[INIT] UWB %s: CS=%u RST=%u IRQ=%u addr=0x%04X\r\n",
                  inst->label,
                  static_cast<unsigned>(inst->pin_cs),
                  static_cast<unsigned>(inst->pin_rst),
                  static_cast<unsigned>(inst->pin_irq),
                  static_cast<unsigned>(short_addr));

    if (!dwm3000_init(inst)) {
        serial_printf("[WARN] UWB %s init failed, dev_id=0x%08lX\r\n",
                      inst->label,
                      static_cast<unsigned long>(dwm3000_read_dev_id(inst)));
        return false;
    }

    dwm3000_configure_default(inst);
    dwm3000_set_antenna_delay(inst, tx_ant_dly, rx_ant_dly);
    dwm3000_write32(inst, DW_PANADR, (static_cast<uint32_t>(TWR_PAN_ID) << 16) | short_addr);
    const int32_t pgf_status = dwm3000_run_pgf_cal(inst);
    serial_printf("[INIT] UWB %s antenna delay: tx=%u rx=%u\r\n",
                  inst->label,
                  static_cast<unsigned>(tx_ant_dly),
                  static_cast<unsigned>(rx_ant_dly));
    serial_printf("[INIT] UWB %s PGF calibration: %ld\r\n",
                  inst->label,
                  static_cast<long>(pgf_status));
    serial_printf("[INIT] UWB %s ready.\r\n", inst->label);
    return true;
}
#endif

void init_mics(void)
{
#if ENABLE_MIC_ADC
    serial_printf("[INIT] MIC sampler: channels=%u scan=%lu us per_ch=%lu Hz block=%u frames\r\n",
                  static_cast<unsigned>(MIC_CHANNEL_COUNT),
                  static_cast<unsigned long>(MIC_SCAN_PERIOD_US),
                  static_cast<unsigned long>(1000000UL / (MIC_SCAN_PERIOD_US * MIC_CHANNEL_COUNT)),
                  static_cast<unsigned>(MIC_BLOCK_FRAMES));

    if (!mic_sampler_init(kMicPins, MIC_CHANNEL_COUNT)) {
        serial_printf("[WARN] MIC sampler init failed.\r\n");
        return;
    }

    if (!mic_sampler_start()) {
        serial_printf("[WARN] MIC sampler start failed.\r\n");
        return;
    }
#endif
}

void service_mics(void)
{
#if ENABLE_MIC_ADC
    mic_sampler_status_t status = {};
    if (!mic_sampler_get_status(&status)) {
        return;
    }

    for (uint8_t i = 0; i < MIC_CHANNEL_COUNT; i++) {
        s_state.mic.raw[i] = status.last_frame[i];
        s_state.mic.mean[i] = status.block_mean[i];
        s_state.mic.peak_to_peak[i] = status.block_peak_to_peak[i];
        s_state.mic.volts[i] =
            (static_cast<float>(kMicRefMv) * static_cast<float>(status.last_frame[i])) / 4095.0f / 1000.0f;
    }

    s_state.mic.tick++;
    s_state.mic.frame_counter = status.frame_counter;
    s_state.mic.block_counter = status.block_counter;
    s_state.mic.block_timestamp_us = status.last_block_timestamp_us;
    s_state.mic.sample_rate_hz = status.per_channel_rate_hz;
    s_state.mic.running = status.running;
    s_state.mic.block_valid = status.block_valid;
#endif
}

void init_imu(void)
{
    serial_printf("[INIT] IMU bring-up...\r\n");
    s_imu_ready = imu_init();
    if (!s_imu_ready) {
        serial_printf("[WARN] IMU init failed.\r\n");
        return;
    }

    serial_printf("[INIT] Keep headset still for IMU calibration...\r\n");
    delay(2000);
    imu_calibrate(CALIBRATION_SAMPLES);
    serial_printf("[INIT] IMU calibration complete.\r\n");
}

void init_filter(void)
{
#if BRINGUP_HAS_FILTER
    eskf_init(&s_eskf);
    eskf_set_uwb_body_pos(&s_eskf, UWB_IDX_T, UWB_T_BX, UWB_T_BY, UWB_T_BZ);
    eskf_set_uwb_body_pos(&s_eskf, UWB_IDX_L, UWB_L_BX, UWB_L_BY, UWB_L_BZ);
    eskf_set_uwb_body_pos(&s_eskf, UWB_IDX_R, UWB_R_BX, UWB_R_BY, UWB_R_BZ);
    eskf_set_tool_pos(&s_eskf, TOOL_WX, TOOL_WY, TOOL_WZ);
    s_filter_ready = true;
    s_filter_time_us = 0;
#endif
}

void init_uwb(void)
{
#if BRINGUP_HAS_UWB_INIT
    serial_printf("[INIT] UWB bring-up on SPI1 @ %lu Hz...\r\n", static_cast<unsigned long>(UWB_SPI_BAUD));
    serial_printf("[INIT] UWB profile: ch=%u sfd=%u pcode=%u psr=%u\r\n",
                  static_cast<unsigned>(UWB_CHANNEL),
                  static_cast<unsigned>(UWB_SFD_TYPE),
                  static_cast<unsigned>(UWB_PREAMBLE_CODE),
                  static_cast<unsigned>(UWB_PREAMBLE_LENGTH));
    dwm3000_spi_bus_init((void *)&SPI1, UWB_SPI_BAUD, UWB_PIN_SCK, UWB_PIN_MOSI, UWB_PIN_MISO);
    uwb_net_set_retry_limit(UWB_MAX_RETRIES);
    serial_printf("[INIT] UWB retry limit: %u\r\n", static_cast<unsigned>(UWB_MAX_RETRIES));

    s_state.uwb_ready[UWB_MODULE_T] =
        init_uwb_module(&s_uwb_t, UWB_ADDR_T, UWB_T_TX_ANT_DLY, UWB_T_RX_ANT_DLY);
    s_state.uwb_ready[UWB_MODULE_L] =
        init_uwb_module(&s_uwb_l, UWB_ADDR_L, UWB_L_TX_ANT_DLY, UWB_L_RX_ANT_DLY);
    s_state.uwb_ready[UWB_MODULE_R] =
        init_uwb_module(&s_uwb_r, UWB_ADDR_R, UWB_R_TX_ANT_DLY, UWB_R_RX_ANT_DLY);
    s_state.uwb_ready[UWB_MODULE_TOOL] =
        init_uwb_module(&s_uwb_tool, UWB_TOOL_ADDR, UWB_TOOL_TX_ANT_DLY, UWB_TOOL_RX_ANT_DLY);

    serial_printf("[INIT] UWB status: top=%u left=%u right=%u tool=%u\r\n",
                  s_state.uwb_ready[UWB_MODULE_T] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_L] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_R] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_TOOL] ? 1u : 0u);

    uwb_net_init(&s_uwb_t, &s_uwb_l, &s_uwb_r, &s_uwb_tool);
    uwb_net_set_uwb_body_pos(UWB_IDX_T, UWB_T_BX, UWB_T_BY, UWB_T_BZ);
    uwb_net_set_uwb_body_pos(UWB_IDX_L, UWB_L_BX, UWB_L_BY, UWB_L_BZ);
    uwb_net_set_uwb_body_pos(UWB_IDX_R, UWB_R_BX, UWB_R_BY, UWB_R_BZ);
#endif
}

void service_imu(void)
{
    if (!s_imu_ready) {
        return;
    }

    imu_sample_t raw_sample = {};
    if (!imu_read_sample(&raw_sample)) {
        return;
    }

    float accel_body_g[3];
    float gyro_body_dps[3];
    rotate_imu_to_body(raw_sample.accel_g, accel_body_g);
    rotate_imu_to_body(raw_sample.gyro_dps, gyro_body_dps);

    s_state.accel_g[0] = accel_body_g[0];
    s_state.accel_g[1] = accel_body_g[1];
    s_state.accel_g[2] = accel_body_g[2];
    s_state.gyro_dps[0] = gyro_body_dps[0];
    s_state.gyro_dps[1] = gyro_body_dps[1];
    s_state.gyro_dps[2] = gyro_body_dps[2];
    s_state.imu_valid = true;
    s_state.imu_tick++;
    s_state.imu_timestamp_us = raw_sample.timestamp_us;

#if BRINGUP_HAS_FILTER
    if (s_filter_ready) {
        if (s_last_imu_sample.valid) {
            predict_filter_to(raw_sample.timestamp_us);
        } else {
            s_filter_time_us = raw_sample.timestamp_us;
        }

        s_last_imu_sample.accel_ms2[0] = accel_body_g[0] * kGravity;
        s_last_imu_sample.accel_ms2[1] = accel_body_g[1] * kGravity;
        s_last_imu_sample.accel_ms2[2] = accel_body_g[2] * kGravity;
        s_last_imu_sample.gyro_rads[0] = gyro_body_dps[0] * (kPi / 180.0f);
        s_last_imu_sample.gyro_rads[1] = gyro_body_dps[1] * (kPi / 180.0f);
        s_last_imu_sample.gyro_rads[2] = gyro_body_dps[2] * (kPi / 180.0f);
        s_last_imu_sample.timestamp_us = raw_sample.timestamp_us;
        s_last_imu_sample.valid = true;
    }
#endif
}

#if BRINGUP_HAS_UWB_RANGING
void service_uwb(void)
{
    const uwb_cycle_result_t cycle = uwb_net_range_masked(active_range_mask());
    for (uint8_t i = 0; i < UWB_NUM_HEADSET; i++) {
        s_state.range_m[i] = cycle.range_m[i];
        s_state.range_valid[i] = cycle.valid[i];
        s_state.range_timestamp_us[i] = cycle.range_timestamp_us[i];
    }
    s_state.uwb_tick++;
    s_state.uwb_cycle_timestamp_us = cycle.timestamp_us;

#if BRINGUP_HAS_FILTER
    if (s_filter_ready && s_last_imu_sample.valid && cycle.num_valid > 0) {
        bool updated = false;
        for (uint8_t i = 0; i < UWB_NUM_HEADSET; i++) {
            if (!cycle.valid[i]) {
                continue;
            }

            predict_filter_to(cycle.range_timestamp_us[i]);
            updated |= eskf_update_range(&s_eskf, i, cycle.range_m[i]);
        }

        if (updated) {
            eskf_inject_and_reset(&s_eskf);
        }

        predict_filter_to(cycle.timestamp_us);
    }
#endif
}
#endif

void emit_telemetry(void)
{
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;

#if BRINGUP_HAS_FILTER
    if (s_filter_ready) {
        pos_x = s_eskf.pos[0];
        pos_y = s_eskf.pos[1];
        pos_z = s_eskf.pos[2];
        eskf_get_euler(&s_eskf, &roll, &pitch, &yaw);
    }
#endif

    serial_printf("$POSE,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f\r\n",
                  pos_x, pos_y, pos_z,
                  roll * kDegPerRad, pitch * kDegPerRad, yaw * kDegPerRad,
                  s_state.range_m[0], s_state.range_m[1], s_state.range_m[2]);

#if ENABLE_MIC_ADC
    serial_printf("$MICS,%u,%u,%u,%u,%u\r\n",
                  static_cast<unsigned>(s_state.mic.raw[MIC_TOOL]),
                  static_cast<unsigned>(s_state.mic.raw[MIC_OUT_L]),
                  static_cast<unsigned>(s_state.mic.raw[MIC_OUT_R]),
                  static_cast<unsigned>(s_state.mic.raw[MIC_IN_L]),
                  static_cast<unsigned>(s_state.mic.raw[MIC_IN_R]));

    serial_printf("$MICDBG,run=%u,valid=%u,hz=%lu,frames=%lu,blocks=%lu,pp=%u,%u,%u,%u,%u,mean=%u,%u,%u,%u,%u\r\n",
                  s_state.mic.running ? 1u : 0u,
                  s_state.mic.block_valid ? 1u : 0u,
                  static_cast<unsigned long>(s_state.mic.sample_rate_hz),
                  static_cast<unsigned long>(s_state.mic.frame_counter),
                  static_cast<unsigned long>(s_state.mic.block_counter),
                  static_cast<unsigned>(s_state.mic.peak_to_peak[MIC_TOOL]),
                  static_cast<unsigned>(s_state.mic.peak_to_peak[MIC_OUT_L]),
                  static_cast<unsigned>(s_state.mic.peak_to_peak[MIC_OUT_R]),
                  static_cast<unsigned>(s_state.mic.peak_to_peak[MIC_IN_L]),
                  static_cast<unsigned>(s_state.mic.peak_to_peak[MIC_IN_R]),
                  static_cast<unsigned>(s_state.mic.mean[MIC_TOOL]),
                  static_cast<unsigned>(s_state.mic.mean[MIC_OUT_L]),
                  static_cast<unsigned>(s_state.mic.mean[MIC_OUT_R]),
                  static_cast<unsigned>(s_state.mic.mean[MIC_IN_L]),
                  static_cast<unsigned>(s_state.mic.mean[MIC_IN_R]));
#endif

#if BRINGUP_HAS_UWB_INIT
    uwb_link_diag_t diag_t = {};
    uwb_link_diag_t diag_l = {};
    uwb_link_diag_t diag_r = {};
    uwb_net_get_link_diag(UWB_IDX_T, &diag_t);
    uwb_net_get_link_diag(UWB_IDX_L, &diag_l);
    uwb_net_get_link_diag(UWB_IDX_R, &diag_r);

    serial_printf("$UWBTS,%lu,%lu,%lu,%lu\r\n",
                  static_cast<unsigned long>(s_state.range_timestamp_us[0]),
                  static_cast<unsigned long>(s_state.range_timestamp_us[1]),
                  static_cast<unsigned long>(s_state.range_timestamp_us[2]),
                  static_cast<unsigned long>(s_state.uwb_cycle_timestamp_us));

    serial_printf("$UWBDBG,st=%u,%u,%u,att=%u,%u,%u,cf=%lu,%lu,%lu,ok=%lu,%lu,%lu,fail=%lu,%lu,%lu\r\n",
                  static_cast<unsigned>(diag_t.last_status),
                  static_cast<unsigned>(diag_l.last_status),
                  static_cast<unsigned>(diag_r.last_status),
                  static_cast<unsigned>(diag_t.last_attempts_used),
                  static_cast<unsigned>(diag_l.last_attempts_used),
                  static_cast<unsigned>(diag_r.last_attempts_used),
                  static_cast<unsigned long>(diag_t.consecutive_failures),
                  static_cast<unsigned long>(diag_l.consecutive_failures),
                  static_cast<unsigned long>(diag_r.consecutive_failures),
                  static_cast<unsigned long>(diag_t.total_successes),
                  static_cast<unsigned long>(diag_l.total_successes),
                  static_cast<unsigned long>(diag_r.total_successes),
                  static_cast<unsigned long>(diag_t.total_failures),
                  static_cast<unsigned long>(diag_l.total_failures),
                  static_cast<unsigned long>(diag_r.total_failures));
#endif

#if BRINGUP_HAS_FILTER
    serial_printf("$STAT,stage=%u,imu=%u,imu_us=%lu,uwb_tick=%lu,flt_us=%lu,acc=%lu,rej=%lu,top=%u,left=%u,right=%u,tool=%u\r\n",
                  static_cast<unsigned>(BRINGUP_STAGE),
                  s_imu_ready ? 1u : 0u,
                  static_cast<unsigned long>(s_state.imu_timestamp_us),
                  static_cast<unsigned long>(s_state.uwb_tick),
                  static_cast<unsigned long>(s_filter_time_us),
                  static_cast<unsigned long>(s_eskf.uwb_accepted),
                  static_cast<unsigned long>(s_eskf.uwb_rejected),
                  s_state.uwb_ready[UWB_MODULE_T] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_L] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_R] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_TOOL] ? 1u : 0u);
#elif BRINGUP_HAS_UWB_INIT
    serial_printf("$STAT,stage=%u,imu=%u,imu_us=%lu,uwb_tick=%lu,top=%u,left=%u,right=%u,tool=%u\r\n",
                  static_cast<unsigned>(BRINGUP_STAGE),
                  s_imu_ready ? 1u : 0u,
                  static_cast<unsigned long>(s_state.imu_timestamp_us),
                  static_cast<unsigned long>(s_state.uwb_tick),
                  s_state.uwb_ready[UWB_MODULE_T] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_L] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_R] ? 1u : 0u,
                  s_state.uwb_ready[UWB_MODULE_TOOL] ? 1u : 0u);
#else
    serial_printf("$STAT,stage=%u,imu=%u,imu_us=%lu,uwb_tick=%lu\r\n",
                  static_cast<unsigned>(BRINGUP_STAGE),
                  s_imu_ready ? 1u : 0u,
                  static_cast<unsigned long>(s_state.imu_timestamp_us),
                  static_cast<unsigned long>(s_state.uwb_tick));
#endif
}

} // namespace

void setup(void)
{
    Serial.begin(115200);
    const uint32_t wait_start = millis();
    while (!Serial && (millis() - wait_start) < 3000U) {
        delay(10);
    }

    serial_printf("\r\n=== Dae Teensy 4.1 sensory front-end ===\r\n");
    serial_printf("Mainboard nets: Documents/ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md\r\n");
    serial_printf("stage=%u (%s) UWB=%u ESKF=%u MIC_ADC=%u\r\n",
                  static_cast<unsigned>(BRINGUP_STAGE),
                  bringup_stage_name(),
                  static_cast<unsigned>(ENABLE_UWB),
                  static_cast<unsigned>(ENABLE_ESKF),
                  static_cast<unsigned>(ENABLE_MIC_ADC));

    init_mics();
    init_imu();
    init_uwb();
    init_filter();
    service_mics();

    const uint32_t now = millis();
    s_next_imu_ms = now;
    s_next_uwb_ms = now;
    s_next_telemetry_ms = now + TELEMETRY_PERIOD_MS;

    serial_printf("[INIT] Entering runtime loop.\r\n");
}

void loop(void)
{
    service_mics();

    const uint32_t now = millis();

    if (static_cast<int32_t>(now - s_next_imu_ms) >= 0) {
        s_next_imu_ms += IMU_LOOP_PERIOD_MS;
        service_imu();
    }

#if BRINGUP_HAS_UWB_RANGING
    if (static_cast<int32_t>(now - s_next_uwb_ms) >= 0) {
        s_next_uwb_ms += UWB_CYCLE_PERIOD_MS;
        service_uwb();
    }
#endif

    if (static_cast<int32_t>(now - s_next_telemetry_ms) >= 0) {
        s_next_telemetry_ms += TELEMETRY_PERIOD_MS;
        emit_telemetry();
    }

    yield();
}
