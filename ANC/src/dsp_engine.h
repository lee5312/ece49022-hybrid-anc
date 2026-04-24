#pragma once

#include "sai_input.h"
#include "sai_output.h"
#include "internal_adc_reader.h"
#include "dsp_bridge.h"
#include "audio_config.h"

class DSPEngine {
public:
    // Main processing function (called every audio block)
    DSPEngine();
    void begin();
    void process();

    SAIOutput output1; //DAC1 (pin 7) - Anti-taget signal (-y_hat)
    SAIOutput output2; //DAC2 (pin 32) - Anti-noise 90-degree shifted signal (-y_hat_90)
    SAIOutput output3; //DAC3 (pin 9) - Speech signal (e)

private:
    // Low-level hardware interfaces
    SAIInput input;

    InternalADCReader adc_reader;

    // The dedicated DSP processing unit
    DSPBridge dsp_bridge;

    // UPDATED: Three separate buffers for the three distinct output signals
    int16_t speech_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];
    int16_t anti_noise_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];
    int16_t anti_noise_90_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];

    // Optional internal working buffer (float is better for DSP)
    float cal_L[AUDIO_BLOCK];
    float fb_L[AUDIO_BLOCK];
    float cal_R[AUDIO_BLOCK];
    float fb_R[AUDIO_BLOCK];

    // Example placeholder for future algorithms
    float output_L[AUDIO_BLOCK];
    float output_R[AUDIO_BLOCK];
};