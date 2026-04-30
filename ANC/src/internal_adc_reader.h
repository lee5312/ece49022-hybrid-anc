#pragma once

#include <Arduino.h>

class InternalADCReader {
public:
    InternalADCReader(uint8_t pin);
    void begin();
    float read(); // Returns a normalized value from 0.0 to 1.0

private:
    uint8_t _pin;
};
