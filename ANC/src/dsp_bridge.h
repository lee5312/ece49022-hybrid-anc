#pragma once

#include <Arduino.h>
#include "audio_config.h"
#include "lms_filter.h"

class DSPBridge {
public:
  DSPBridge(); 
  void begin();
    void process(const int32_t* input, float tool_adc_val,
                 int32_t* speech_out, 
                 int32_t* anti_noise_out, 
                 int32_t* anti_noise_90_out);
private:
  float gain = 1.0f;

  LMSFilter lms_left;
  LMSFilter lms_right;
};
