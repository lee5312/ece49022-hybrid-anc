#ifndef MIC_SAMPLER_H
#define MIC_SAMPLER_H

#include <stdbool.h>
#include <stdint.h>

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool initialized;
    bool running;
    bool block_valid;
    uint8_t channel_count;
    uint32_t scan_period_us;
    uint32_t per_channel_rate_hz;
    uint32_t frame_counter;
    uint32_t block_counter;
    uint32_t last_block_timestamp_us;
    uint16_t last_frame[MIC_CHANNEL_COUNT];
    uint16_t block_mean[MIC_CHANNEL_COUNT];
    uint16_t block_min[MIC_CHANNEL_COUNT];
    uint16_t block_max[MIC_CHANNEL_COUNT];
    uint16_t block_peak_to_peak[MIC_CHANNEL_COUNT];
} mic_sampler_status_t;

bool mic_sampler_init(const uint8_t *pins, uint8_t channel_count);
bool mic_sampler_start(void);
void mic_sampler_stop(void);
bool mic_sampler_get_status(mic_sampler_status_t *status);

#ifdef __cplusplus
}
#endif

#endif // MIC_SAMPLER_H
