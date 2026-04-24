#include <Arduino.h>
#include "clock_and_pin_config.h" // We need this to set up the clocks/pins
#include "imxrt.h"                // We need this for direct register access

void setup() {
    Serial.begin(9600);
    while (!Serial) {}
    Serial.println("Minimal Hardware Clock Enable Test");

    // Step 1: Configure all clocks and pins
    // This calls your clock_and_pin_config.cpp file
    init_audio_clocks_and_pins();
    Serial.println("Clock and pin configuration complete.");

    // Step 2: Manually enable the SAI1 Transmitter
    // This is the most critical part. We are directly writing to the hardware registers.
    
    // Configure the transmitter for master mode.
    // BCD=1 (Bit Clock is output), BCE=1 (Bit Clock is enabled)
    I2S1_TCR2 = I2S_TCR2_BCD | I2S_TCR2_BCP; // BCP for clock polarity
    I2S1_TCR4 = I2S_TCR4_FSD;               // FSD=1 (Frame Sync is output)
    
    // Enable the Transmitter (TE) and Bit Clock (BCE)
    I2S1_TCSR = I2S_TCSR_TE | I2S_TCSR_BCE;

    Serial.println("SAI1 Transmitter manually enabled. Clocks should be active NOW.");
    Serial.println("Check Pins 20, 21, and 23 with the oscilloscope.");
}

void loop() {
    // The loop is empty. The clocks are controlled by hardware and
    // will continue running forever after being enabled in setup().
    Serial.println("Still running...");
    delay(2000);
}
