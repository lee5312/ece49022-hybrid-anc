#include "mic_sampler.h"

#include <Arduino.h>
#include <IntervalTimer.h>

namespace {

IntervalTimer s_timer;

volatile bool s_initialized = false;
volatile bool s_running = false;
volatile bool s_block_valid = false;

uint8_t s_pins[MIC_CHANNEL_COUNT] = {};
volatile uint8_t s_channel_count = 0;
volatile uint8_t s_scan_channel = 0;
volatile uint16_t s_frames_in_block = 0;
volatile uint32_t s_frame_counter = 0;
volatile uint32_t s_block_counter = 0;
volatile uint32_t s_last_block_timestamp_us = 0;

volatile uint16_t s_last_frame[MIC_CHANNEL_COUNT] = {};
volatile uint32_t s_block_sum[MIC_CHANNEL_COUNT] = {};
volatile uint16_t s_block_min[MIC_CHANNEL_COUNT] = {};
volatile uint16_t s_block_max[MIC_CHANNEL_COUNT] = {};

volatile uint16_t s_latest_mean[MIC_CHANNEL_COUNT] = {};
volatile uint16_t s_latest_min[MIC_CHANNEL_COUNT] = {};
volatile uint16_t s_latest_max[MIC_CHANNEL_COUNT] = {};
volatile uint16_t s_latest_peak_to_peak[MIC_CHANNEL_COUNT] = {};

void reset_block_accumulators(void)
{
    s_frames_in_block = 0;
    for (uint8_t i = 0; i < MIC_CHANNEL_COUNT; i++) {
        s_block_sum[i] = 0;
        s_block_min[i] = 0xFFFFu;
        s_block_max[i] = 0u;
    }
}

void sample_isr(void)
{
    const uint8_t channel_count = s_channel_count;
    if (!s_running || channel_count == 0U) {
        return;
    }

    const uint8_t channel = s_scan_channel;
    const uint16_t raw = static_cast<uint16_t>(analogRead(s_pins[channel]));

    s_last_frame[channel] = raw;
    s_block_sum[channel] += raw;
    if (raw < s_block_min[channel]) {
        s_block_min[channel] = raw;
    }
    if (raw > s_block_max[channel]) {
        s_block_max[channel] = raw;
    }

    uint8_t next_channel = channel + 1U;
    if (next_channel < channel_count) {
        s_scan_channel = next_channel;
        return;
    }

    s_scan_channel = 0U;
    s_frame_counter++;
    s_frames_in_block++;

    if (s_frames_in_block < MIC_BLOCK_FRAMES) {
        return;
    }

    s_last_block_timestamp_us = micros();
    s_block_counter++;
    s_block_valid = true;

    for (uint8_t i = 0; i < channel_count; i++) {
        const uint16_t min_value = s_block_min[i];
        const uint16_t max_value = s_block_max[i];
        s_latest_mean[i] = static_cast<uint16_t>(s_block_sum[i] / MIC_BLOCK_FRAMES);
        s_latest_min[i] = min_value;
        s_latest_max[i] = max_value;
        s_latest_peak_to_peak[i] = static_cast<uint16_t>(max_value - min_value);
    }

    reset_block_accumulators();
}

} // namespace

bool mic_sampler_init(const uint8_t *pins, uint8_t channel_count)
{
    if (!pins || channel_count == 0U || channel_count > MIC_CHANNEL_COUNT) {
        return false;
    }

    noInterrupts();
    s_channel_count = channel_count;
    s_scan_channel = 0U;
    s_frame_counter = 0U;
    s_block_counter = 0U;
    s_last_block_timestamp_us = 0U;
    s_block_valid = false;
    s_running = false;
    interrupts();

    analogReadResolution(MIC_ADC_RESOLUTION_BITS);
    analogReadAveraging(MIC_ADC_AVERAGING);

    for (uint8_t i = 0; i < channel_count; i++) {
        s_pins[i] = pins[i];
        pinMode(s_pins[i], INPUT);
        s_last_frame[i] = 0U;
        s_latest_mean[i] = 0U;
        s_latest_min[i] = 0U;
        s_latest_max[i] = 0U;
        s_latest_peak_to_peak[i] = 0U;
    }

    noInterrupts();
    reset_block_accumulators();
    s_initialized = true;
    interrupts();
    return true;
}

bool mic_sampler_start(void)
{
    if (!s_initialized) {
        return false;
    }

    noInterrupts();
    s_scan_channel = 0U;
    reset_block_accumulators();
    s_running = true;
    interrupts();

    return s_timer.begin(sample_isr, MIC_SCAN_PERIOD_US);
}

void mic_sampler_stop(void)
{
    s_timer.end();
    noInterrupts();
    s_running = false;
    interrupts();
}

bool mic_sampler_get_status(mic_sampler_status_t *status)
{
    if (!status || !s_initialized) {
        return false;
    }

    noInterrupts();
    status->initialized = s_initialized;
    status->running = s_running;
    status->block_valid = s_block_valid;
    status->channel_count = s_channel_count;
    status->scan_period_us = MIC_SCAN_PERIOD_US;
    status->per_channel_rate_hz =
        (s_channel_count > 0U) ? (1000000UL / (MIC_SCAN_PERIOD_US * s_channel_count)) : 0U;
    status->frame_counter = s_frame_counter;
    status->block_counter = s_block_counter;
    status->last_block_timestamp_us = s_last_block_timestamp_us;

    for (uint8_t i = 0; i < MIC_CHANNEL_COUNT; i++) {
        status->last_frame[i] = s_last_frame[i];
        status->block_mean[i] = s_latest_mean[i];
        status->block_min[i] = s_latest_min[i];
        status->block_max[i] = s_latest_max[i];
        status->block_peak_to_peak[i] = s_latest_peak_to_peak[i];
    }
    interrupts();

    return true;
}
