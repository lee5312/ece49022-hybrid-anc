#include "dsp_engine.h"
#include "clock_and_pin_config.h"
#include "imxrt.h"


DSPEngine::DSPEngine() :
    input(),
    output1(&I2S1, true),       // SAI1 is Master
    output2(&I2S2, false),      // SAI2 is Slave
    output3(&I2S3, false),      // SAI3 is Slave
    adc_reader(A3),             // Using pin A3 for the internal ADC
    dsp_bridge()
{}
// ======================================================
// INIT DSP ENGINE
// ======================================================
void DSPEngine::begin() {
    // 1. Initialize master clocks and hardware pins
    init_audio_clocks_and_pins();
    
    // 2. Initialize all hardware I/O
    input.begin();
    output1.begin();
    output2.begin();
    output3.begin();
    adc_reader.begin();

    // 3. Initialize the DSP logic
    dsp_bridge.begin();
}


// ======================================================
// MAIN DSP PROCESSING FUNCTION
// ======================================================
// This is called once per audio block (128 samples)
void DSPEngine::process() {

     // Only run if a new block of audio data is available from the input
    if (input.available()) {
        const int16_t* in_buffer = input.read();
        if (!in_buffer) return;

        float adc_value = adc_reader.read();

        // UPDATED: Call the bridge to fill all three buffers
        dsp_bridge.process(in_buffer, adc_value, 
                           speech_buffer, 
                           anti_noise_buffer, 
                           anti_noise_90_buffer);

        Serial.println("DSP Engine Function Initialized.");

       // --- THIS IS THE DEBUGGING CODE TO ADD ---

        // Pick one sample from the middle of the block to inspect
        int sample_idx = AUDIO_BLOCK / 2;
        
        // Get the value of the raw Cal_R input for that sample
        int16_t cal_r_in = in_buffer[sample_idx * AUDIO_CHANNELS_IN + CH_CAL_R];

        // Get the final output values for the right channel for that sample
        int16_t speech_r_out = speech_buffer[sample_idx * AUDIO_CHANNELS_OUT + 1];
        int16_t anti_noise_r_out = anti_noise_buffer[sample_idx * AUDIO_CHANNELS_OUT + 1];
        int16_t anti_noise_90_r_out = anti_noise_90_buffer[sample_idx * AUDIO_CHANNELS_OUT + 1];

        // Print all values on one line, separated by commas.
        // This format is perfect for the Serial Plotter.
        Serial.printf("Cal_R_In:%d,Speech_Out:%d,Anti_Noise_Out:%d,Anti_90_Out:%d\n",
                      cal_r_in,
                      speech_r_out,
                      anti_noise_r_out,
                      anti_noise_90_r_out);
        
        // --- END OF DEBUGGING CODE ---

        

        // UPDATED: Route the correct buffer to the correct DAC
        // Your pin mapping: Pin 7_out1A -> output1, Pin 32_out1B -> output2, Pin 9_out1C -> output3
        output1.write(anti_noise_buffer);     // DAC 1 gets Anti-Noise
        output2.write(anti_noise_90_buffer);  // DAC 2 gets Shifted Anti-Noise
        output3.write(speech_buffer);         // DAC 3 gets the clean Speech signal
    }


    // --------------------------------------------------
    // STEP 2: PLACEHOLDER DSP OPERATIONS
    // --------------------------------------------------
    // This is where your future work goes:
    // - adaptive filtering
    // - phase alignment
    // - feedback cancellation
    // - calibration correction

    // --------------------------------------------------
    // STEP 3: (future) send to DAC layer
    // --------------------------------------------------
    // Not implemented yet, but this is where output goes
}