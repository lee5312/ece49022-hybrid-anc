#include "dsp_bridge.h"

// Define LMS parameters
const int LMS_TAPS = 128;
const float LMS_MU = 0.005f;

DSPBridge::DSPBridge() :     
    gain(1.0f),
    lms_left(LMS_TAPS, LMS_MU),
    lms_right(LMS_TAPS, LMS_MU) 
    {
    // Constructor - initialize members if needed
}

void DSPBridge::begin() {
    // This is where you would initialize filters, lookup tables, etc.
    // For now, we just set a default gain.
    lms_left.init();
    lms_right.init();
    gain = 1.0f; // Set a default gain of 1.0
}

// UPDATED IMPLEMENTATION: Fills all three distinct output buffers
void DSPBridge::process(const int32_t* input, float tool_adc_val,
                     int32_t* speech_out, 
                     int32_t* anti_noise_out, 
                     int32_t* anti_noise_90_out) {

    // Per-sample variables
    float cal_L, cal_R;
    float speech_L, speech_R;
    float anti_noise_L, anti_noise_R;
    float anti_noise_90_L, anti_noise_90_R;

    //Serial.println("DSP Bridge Process.");

    for (int i = 0; i < AUDIO_BLOCK; i++) {
        // 1. DEINTERLEAVE
        int base = i * AUDIO_CHANNELS_IN;
        cal_L = (float)input[base + CH_CAL_L];
        // cal_R = (float)input[base + CH_CAL_R];

        // 2. PROCESS with LMS filter for Left and Right channels
        speech_L = lms_left.process(tool_adc_val, cal_L);
        // speech_R = lms_right.process(tool_adc_val, cal_R);
        
        // 3. GET the other signals from the filters
        anti_noise_L = lms_left.get_anti_noise();
        // anti_noise_R = lms_right.get_anti_noise();
        anti_noise_90_L = lms_left.get_anti_noise_90();
        // anti_noise_90_R = lms_right.get_anti_noise_90();
        
        // 4. INTERLEAVE AND WRITE to the appropriate output buffers
        
        // Buffer 1: Speech Signal (e)
        speech_out[i * 2 + 0] = (int32_t)constrain(speech_L * gain, -32768.0f, 32767.0f);
        // speech_out[i * 2 + 1] = (int32_t)constrain(speech_R * gain, -32768.0f, 32767.0f);

        // Buffer 2: Anti-Noise Signal (-y_hat)
        anti_noise_out[i * 2 + 0] = (int32_t)constrain(anti_noise_L * gain, -32768.0f, 32767.0f);
        // anti_noise_out[i * 2 + 1] = (int32_t)constrain(anti_noise_R * gain, -32768.0f, 32767.0f);

        // Buffer 3: Shifted Anti-Noise Signal
        anti_noise_90_out[i * 2 + 0] = (int32_t)constrain(anti_noise_90_L * gain, -32768.0f, 32767.0f);
        // anti_noise_90_out[i * 2 + 1] = (int32_t)constrain(anti_noise_90_R * gain, -32768.0f, 32767.0f);
    }
    //Serial.println("DSP Bridge Process Complete.");
}