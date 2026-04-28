#include <Arduino.h>
#include "dsp_engine.h"

// We need access to AUDIO_BLOCK and AUDIO_CHANNELS_OUT
#include "audio_config.h" 

// Create the single DSPEngine object
DSPEngine dsp;

// --- Test Tone Generation Variables ---

// We now need THREE separate buffers, one for each DAC's stereo signal.
int32_t dac1_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];
int32_t dac2_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];
int32_t dac3_buffer[AUDIO_BLOCK * AUDIO_CHANNELS_OUT];

// Use different phases/frequencies to generate unique signals for each DAC
float phase1 = 0.0;
float phase2 = 0.0;
float phase3 = 0.0;

// DAC 1 will get 440 Hz
float increment1 = 2.0 * PI * 440.0 / AUDIO_SAMPLE_RATE; 
// DAC 2 will get 880 Hz (one octave higher)
float increment2 = 2.0 * PI * 880.0 / AUDIO_SAMPLE_RATE;
// DAC 3 will get 1320 Hz
float increment3 = 2.0 * PI * 1320.0 / AUDIO_SAMPLE_RATE;


void setup() {
    Serial.begin(9600);

    while (!Serial) {}
    // while (!Serial && (millis() < 4000)) {}

    Serial.println("--- Multi-Channel SAI DAC Test ---");
    
    // dsp.begin() initializes all the hardware: clocks, pins, and the single SAI output.
    dsp.begin();
    
    Serial.println("DSP Engine Initialized. Tones should be generating now.");
}

void loop() {
    // 1. Generate three unique blocks of test data
    for (int i = 0; i < AUDIO_BLOCK; i++) {
        // --- Generate sample for DAC 1 (440 Hz) ---
        int32_t sample1 = (int32_t)(sin(phase1) * 1000000000.0);
        phase1 += increment1;
        if (phase1 >= 2.0 * PI) phase1 -= 2.0 * PI;
        dac1_buffer[i * AUDIO_CHANNELS_OUT + 0] = sample1; // Left
        dac1_buffer[i * AUDIO_CHANNELS_OUT + 1] = sample1; // Right

        // --- Generate sample for DAC 2 (880 Hz) ---
        int32_t sample2 = (int32_t)(sin(phase2) * 1000000000.0);
        phase2 += increment2;
        if (phase2 >= 2.0 * PI) phase2 -= 2.0 * PI;
        dac2_buffer[i * AUDIO_CHANNELS_OUT + 0] = sample2; // Left
        dac2_buffer[i * AUDIO_CHANNELS_OUT + 1] = sample2; // Right

        // --- Generate sample for DAC 3 (1320 Hz) ---
        int32_t sample3 = (int32_t)(sin(phase3) * 1000000000.0);
        phase3 += increment3;
        if (phase3 >= 2.0 * PI) phase3 -= 2.0 * PI;
        dac3_buffer[i * AUDIO_CHANNELS_OUT + 0] = sample3; // Left
        dac3_buffer[i * AUDIO_CHANNELS_OUT + 1] = sample3; // Right
    }

   unsigned long current_time = millis();
    if (current_time - last_print_time > 1000) { // Print once per second
        Serial.printf("dac1[0]=%d\n", dac1_buffer[0]);
        last_print_time = current_time;
    }
    // 2. Make a SINGLE call to the new write function, passing all three buffers.
    // The SAIOutput class will handle interleaving them for the DMA.
    dsp.output.write(dac1_buffer, dac2_buffer, dac3_buffer);

    // NO DELAY! The loop needs to run as fast as possible to keep the DMA buffers full.
    // The SAIOutput::write function is non-blocking and will manage the timing.
}
