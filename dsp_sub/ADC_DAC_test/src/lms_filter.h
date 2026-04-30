#ifndef LMS_FILTER_H
#define LMS_FILTER_H

#include <stdint.h> // For integer types

// --- LMS Filter Data Structure ---
// This struct holds all the state variables for a single filter instance.
typedef struct {
    int M;                 // Number of filter taps
    float mu;              // LMS learning rate
    
    float* g;              // Filter coefficients (adaptive weights)
    float* x_buffer;       // Circular buffer for the reference input signal
    float* anti_noise_buffer; // Circular buffer for the anti-noise signal
    
    // Internal state variables
    float e;               // Error signal
    float y_hat;           // Estimated output
    float anti_noise;      // Generated anti-noise signal
    float anti_noise_90;   // 90-degree phase-shifted anti-noise
    
    // Circular buffer indices
    int head_x;
    int head_anti;

} LMSFilter;

// --- Function Prototypes ---

/**
 * @brief Allocates memory for a new LMSFilter instance and its buffers.
 * 
 * @param M_taps The number of taps (filter length).
 * @param mu_rate The learning rate for the LMS algorithm.
 * @return A pointer to the newly created LMSFilter, or NULL if allocation fails.
 */
LMSFilter* lms_filter_create(int M_taps, float mu_rate);

/**
 * @brief Frees the memory allocated for an LMSFilter instance.
 * 
 * @param filter A pointer to the LMSFilter instance to be destroyed.
 */
void lms_filter_destroy(LMSFilter* filter);

/**
 * @brief Resets the filter's state and buffers to zero.
 * 
 * @param filter A pointer to the LMSFilter instance to initialize.
 */
void lms_filter_init(LMSFilter* filter);

/**
 * @brief Processes one sample of the reference and measured signals.
 * 
 * This function performs the core LMS and Hilbert transform operations.
 * 
 * @param filter A pointer to the LMSFilter instance.
 * @param reference_input The input signal (e.g., from a reference microphone).
 * @param measured_signal The signal to be cancelled (e.g., from an error microphone).
 * @return The error signal `e`, which is the primary output for cancellation.
 */
float lms_filter_process(LMSFilter* filter, float reference_input, float measured_signal);

#endif // LMS_FILTER_H

