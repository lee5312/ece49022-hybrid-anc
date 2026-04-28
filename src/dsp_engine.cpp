#include "dsp_engine.h"
#include "clock_and_pin_config.h"
#include "imxrt.h"

// ======================================================
// CONSTRUCTOR
// ======================================================
DSPEngine::DSPEngine() :
    input(),
    // CORRECTED: Initialize the single, multi-channel output object.
    // The constructor no longer takes a boolean 'isMaster' flag.
    output(&I2S1), 
    adc_reader(A3),
    dsp_bridge()
{}

// ======================================================
// INIT DSP ENGINE
// ======================================================
void DSPEngine::begin() {
    // 1. Initialize master clocks and hardware pins
    init_audio_clocks_and_pins();
    Serial.println("Clock and pin configuration complete.");
    
    // 2. Initialize all hardware I/O
    input.begin();
    // CORRECTED: Call begin() on the single output object.
    output.begin(); 
    adc_reader.begin();
    
    // 3. Initialize the DSP logic
    dsp_bridge.begin();
}

// ======================================================
// MAIN DSP PROCESSING FUNCTION
// ======================================================
void DSPEngine::process() {
    if (input.available()) {
        const int32_t* in_buffer = input.read();
        if (!in_buffer) return;

        float adc_value = adc_reader.read();

        // The DSP bridge still correctly fills our three separate buffers.
        dsp_bridge.process(in_buffer, adc_value, 
                           speech_buffer, 
                           anti_noise_buffer, 
                           anti_noise_90_buffer);
        
        // (Your serial printing for debugging can remain here if you wish)

        // ==============================================================
        // CORRECTED: The single 'write' call for the multi-channel output
        // ==============================================================
        // We now pass all three processed buffers to the single 'write'
        // function, which will handle interleaving the data for the DMA.
        // Let's match the physical DAC connections you wanted:
        // DAC 1 (e.g., Pin 7) -> Anti-Noise
        // DAC 2 (e.g., Pin 32) -> Shifted Anti-Noise
        // DAC 3 (e.g., Pin 9) -> Clean Speech
        output.write(anti_noise_buffer, anti_noise_90_buffer, speech_buffer);
    }
}
