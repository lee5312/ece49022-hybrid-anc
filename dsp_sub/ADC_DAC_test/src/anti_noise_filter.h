#ifndef ANTI_NOISE_FILTER_H
#define ANTI_NOISE_FILTER_H

// --- Delay Filter Structure ---
typedef struct {
    float gain;          // Gain multiplier for the output signal
    int delay_samples;   // The current delay in number of samples

    int buffer_size;     // The maximum supported delay + 1
    float* delay_buffer; // Circular buffer for the delay line
    int head;            // Current write position in the buffer

} AntiNoiseFilter;


// --- Public Function Declarations ---

/**
 * @brief Allocates and initializes a new delay-and-gain filter.
 * @param max_delay_samples The maximum number of samples you want to be able to delay by.
 * @return Pointer to the new filter, or NULL if allocation fails.
 */
AntiNoiseFilter* anti_noise_filter_create(int max_delay_samples);

/**
 * @brief Frees all memory associated with a filter.
 * @param filter Pointer to the filter to destroy.
 */
void anti_noise_filter_destroy(AntiNoiseFilter* filter);

/**
 * @brief Sets the output gain.
 * @param filter Pointer to the filter.
 * @param gain The desired gain (e.g., 1.0 for 100%).
 */
void anti_noise_filter_set_gain(AntiNoiseFilter* filter, float gain);

/**
 * @brief Sets the output delay in samples.
 * @param filter Pointer to the filter.
 * @param delay_samples The number of samples to delay the signal by. Must be less than `max_delay_samples`.
 */
void anti_noise_filter_set_delay(AntiNoiseFilter* filter, int delay_samples);

/**
 * @brief Processes one sample, applying delay and gain.
 * @param filter Pointer to the filter.
 * @param reference_input The clean reference signal.
 * @param measured_signal Ignored in this mode.
 * @return The delayed and inverted anti-noise signal with gain applied.
 */
float anti_noise_filter_process(AntiNoiseFilter* filter, float reference_input, float measured_signal);

#endif // ANTI_NOISE_FILTER_H
