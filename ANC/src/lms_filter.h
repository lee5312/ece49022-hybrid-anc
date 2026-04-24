#pragma once

#include <cstdint>

class LMSFilter {
public:
    // M = Filter length (number of taps)
    // mu = Learning rate
    LMSFilter(int M, float mu);
    ~LMSFilter(); // Destructor to free memory

    void init();
    float process(float reference_input, float measured_signal);

    // Allows reading the current error signal after processing
    float get_error() const { return e; }
    float get_y_hat() const { return y_hat; }
    float get_anti_noise() const { return anti_noise; }
    float get_anti_noise_90() const { return anti_noise_90; }

private:
    const int M;   // Filter length
    const float mu; // Learning rate

    float* g;       // Filter coefficients (taps)
    float* x_buffer; // Input signal buffer (circular)
    float* anti_noise_buffer;
    static const float h_hilbert[128]; // Use M directly if it's always 128


    float e;        // Current error
    float y_hat;    // Current estimated output
    float anti_noise;
    float anti_noise_90;


    // Circular buffer heads
    int head_x;
    int head_anti;
};
