#include "anti_noise_filter.h"
#include <stdlib.h> // For malloc(), free()
#include <string.h> // For memset()

AntiNoiseFilter* anti_noise_filter_create(int max_delay_samples) {
    AntiNoiseFilter* filter = (AntiNoiseFilter*)malloc(sizeof(AntiNoiseFilter));
    if (filter == NULL) return NULL;

    // We need space for max_delay + the current sample
    filter->buffer_size = max_delay_samples + 1; 

    // Initialize parameters
    filter->gain = 1.0f;
    filter->delay_samples = 0; // Default to no delay
    filter->head = 0;

    filter->delay_buffer = (float*)malloc(sizeof(float) * filter->buffer_size);
    if (filter->delay_buffer == NULL) {
        free(filter);
        return NULL;
    }
    
    // Clear the buffer to prevent garbage values at the start
    memset(filter->delay_buffer, 0, sizeof(float) * filter->buffer_size);

    return filter;
}

void anti_noise_filter_destroy(AntiNoiseFilter* filter) {
    if (filter != NULL) {
        free(filter->delay_buffer);
        free(filter);
    }
}

void anti_noise_filter_set_gain(AntiNoiseFilter* filter, float gain) {
    if (filter == NULL) return;
    filter->gain = gain;
}

void anti_noise_filter_set_delay(AntiNoiseFilter* filter, int delay_samples) {
    if (filter == NULL) return;

    // Ensure the requested delay is not larger than the buffer can handle
    if (delay_samples < filter->buffer_size) {
        filter->delay_samples = delay_samples;
    } else {
        // If requested delay is too large, set to maximum possible
        filter->delay_samples = filter->buffer_size - 1;
    }
}

float anti_noise_filter_process(AntiNoiseFilter* filter, float reference_input, float measured_signal) {
    if (filter == NULL) return 0.0f;

    // 1. Store the current input sample at the head of the delay line
    filter->delay_buffer[filter->head] = reference_input;

    // 2. Calculate the read position for the delayed sample
    // This is where we "look back" in time (the buffer).
    int read_index = filter->head - filter->delay_samples;

    // Handle circular buffer wrap-around if the index is negative
    if (read_index < 0) {
        read_index += filter->buffer_size;
    }

    // 3. Get the delayed input value
    float delayed_input = filter->delay_buffer[read_index];

    // 4. Calculate the final output using the delayed sample
    float final_output = -delayed_input * filter->gain;

    // 5. Advance the head for the next sample
    filter->head++;
    if (filter->head >= filter->buffer_size) {
        filter->head = 0; // Wrap around to the start of the buffer
    }

    return final_output;
}
