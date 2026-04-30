#pragma once

#include <cstdint>

constexpr uint32_t AUDIO_SAMPLE_RATE = 48000;
constexpr size_t AUDIO_FRAME_SIZE = 128;
constexpr size_t AUDIO_CHANNELS_IN = 3;   // 2 external ADCs + 1 internal ADC
constexpr size_t AUDIO_CHANNELS_OUT = 3;  // 3 external DACs
constexpr size_t AUDIO_DMA_BUFFER_FRAMES = AUDIO_FRAME_SIZE;
