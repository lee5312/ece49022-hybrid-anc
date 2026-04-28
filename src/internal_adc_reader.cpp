#include "internal_adc_reader.h"

InternalADCReader::InternalADCReader(uint8_t pin) : _pin(pin) {}

void InternalADCReader::begin() {
    pinMode(_pin, INPUT);
    analogReadResolution(12); // Use 12-bit resolution for better quality
}

float InternalADCReader::read() {
    // analogRead returns 0-4095 for 12-bit resolution
    return (float)analogRead(_pin) / 4095.0f;
}
