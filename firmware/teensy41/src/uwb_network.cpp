#include "uwb_network.h"

#include "uwb_ranging.h"

#include <Arduino.h>
#include <string.h>

namespace {

dwm3000_inst_t *s_uwb[UWB_NUM_HEADSET] = { nullptr, nullptr, nullptr };
dwm3000_inst_t *s_tool = nullptr;
uwb_link_diag_t s_diag[UWB_NUM_HEADSET] = {};
uint8_t s_retry_limit = 0;

uint16_t s_uwb_addrs[UWB_NUM_HEADSET] = {
    UWB_ADDR_T,
    UWB_ADDR_L,
    UWB_ADDR_R,
};

uint16_t s_tool_addr = UWB_TOOL_ADDR;

float s_uwb_body_pos[UWB_NUM_HEADSET][3] = {
    {  0.00f, 0.00f, 0.10f },
    { -0.08f, 0.00f, 0.00f },
    {  0.08f, 0.00f, 0.00f },
};

constexpr uint32_t SLOT_GUARD_US = 200;

} // namespace

void uwb_net_init(dwm3000_inst_t *t, dwm3000_inst_t *l,
                  dwm3000_inst_t *r, dwm3000_inst_t *tool)
{
    s_uwb[UWB_IDX_T] = t;
    s_uwb[UWB_IDX_L] = l;
    s_uwb[UWB_IDX_R] = r;
    s_tool = tool;
}

void uwb_net_set_uwb_body_pos(uint8_t uwb_idx, float x, float y, float z)
{
    if (uwb_idx >= UWB_NUM_HEADSET) {
        return;
    }
    s_uwb_body_pos[uwb_idx][0] = x;
    s_uwb_body_pos[uwb_idx][1] = y;
    s_uwb_body_pos[uwb_idx][2] = z;
}

void uwb_net_get_uwb_body_pos(uint8_t uwb_idx, float *x, float *y, float *z)
{
    if (uwb_idx >= UWB_NUM_HEADSET) {
        return;
    }
    if (x) *x = s_uwb_body_pos[uwb_idx][0];
    if (y) *y = s_uwb_body_pos[uwb_idx][1];
    if (z) *z = s_uwb_body_pos[uwb_idx][2];
}

void uwb_net_set_retry_limit(uint8_t retries)
{
    s_retry_limit = retries;
}

void uwb_net_get_link_diag(uint8_t uwb_idx, uwb_link_diag_t *diag)
{
    if (!diag) {
        return;
    }

    memset(diag, 0, sizeof(*diag));
    if (uwb_idx >= UWB_NUM_HEADSET) {
        return;
    }

    *diag = s_diag[uwb_idx];
}

uwb_cycle_result_t uwb_net_range_cycle(void)
{
    return uwb_net_range_masked(UWB_RANGE_MASK_ALL);
}

uwb_cycle_result_t uwb_net_range_masked(uint8_t range_mask)
{
    uwb_cycle_result_t result = {};

    for (int i = 0; i < UWB_NUM_HEADSET; i++) {
        result.range_timestamp_us[i] = 0;

        if ((range_mask & (1u << i)) == 0u) {
            result.range_m[i] = 0.0f;
            result.valid[i] = false;
            continue;
        }

        s_diag[i].last_attempt_timestamp_us = micros();
        s_diag[i].last_attempts_used = 0;
        s_diag[i].last_valid = false;
        s_diag[i].last_status = static_cast<uint8_t>(TWR_STATUS_NOT_READY);

        if (!s_uwb[i] || !s_uwb[i]->ready || !s_tool || !s_tool->ready) {
            result.range_m[i] = 0.0f;
            result.valid[i] = false;
            result.range_timestamp_us[i] = micros();
            s_diag[i].total_attempts++;
            s_diag[i].total_failures++;
            s_diag[i].consecutive_failures++;
            s_diag[i].last_attempts_used = 1;
            s_diag[i].last_status = static_cast<uint8_t>(TWR_STATUS_NOT_READY);
            continue;
        }

        twr_result_t twr = { false, 0.0f, 0.0f, s_tool_addr, TWR_STATUS_RESP_RX_TIMEOUT };
        uint8_t attempts_used = 0;
        for (uint8_t attempt = 0; attempt <= s_retry_limit; attempt++) {
            twr = twr_loopback(s_uwb[i], s_uwb_addrs[i], s_tool, s_tool_addr);
            attempts_used = attempt + 1U;
            if (twr.valid) {
                break;
            }
        }

        const uint32_t stamp_us = micros();
        result.range_m[i] = twr.distance_m;
        result.valid[i] = twr.valid;
        result.range_timestamp_us[i] = stamp_us;
        s_diag[i].total_attempts += attempts_used;
        s_diag[i].last_attempt_timestamp_us = stamp_us;
        s_diag[i].last_attempts_used = attempts_used;
        s_diag[i].last_status = static_cast<uint8_t>(twr.status);
        s_diag[i].last_valid = twr.valid;
        if (twr.valid) {
            result.num_valid++;
            s_diag[i].total_successes++;
            s_diag[i].consecutive_failures = 0;
            s_diag[i].last_distance_m = twr.distance_m;
            s_diag[i].last_success_timestamp_us = stamp_us;
        } else {
            s_diag[i].total_failures++;
            s_diag[i].consecutive_failures++;
        }

        delayMicroseconds(SLOT_GUARD_US);
    }

    result.timestamp_us = micros();
    return result;
}
