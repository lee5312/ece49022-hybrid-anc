#include <Arduino.h>
#include "dsp_engine.h"

// Create the single DSPEngine object.
// No other global variables are needed here anymore.
DSPEngine dsp;

void setup() {
    pinMode(LED_BUILTIN, OUTPUT); // Keep this for easy visual debugging.
    Serial.begin(9600);
    while (!Serial) {
      // wait for serial port to connect.
    }

    Serial.println("--- DSP Engine Test ---");
    
    // dsp.begin() initializes all the hardware and the DSPEngine.
    dsp.begin();
    
    Serial.println("DSP Engine Initialized. Starting processing loop.");
}

void loop() {
    // Toggle the LED to confirm the loop is running.
    Serial.println("Still running...");
    //digitalToggle(LED_BUILTIN); 

    // The ONLY thing we need to do now is call process().
    // The DSPEngine will generate its own internal test tone,
    // process it, and send it to the output.
    dsp.process(); 
    delay(2000); // Add a delay to slow down the loop for testing.
}
