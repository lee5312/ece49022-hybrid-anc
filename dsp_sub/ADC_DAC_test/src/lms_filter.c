#include "lms_filter.h"
#include <stdlib.h> // For malloc() and free()
#include <string.h> // For memset()


// For debugging purposes. You might want to remove or replace this.
#include <stdio.h> 

// MATLAB-generated Hilbert FIR coefficients.
// These are constant and shared by all filter instances.
static const float h_hilbert[128] = {
    -0.0000130075f, -0.0000022259f, -0.0000226577f, -0.0000060656f, -0.0000418038f, -0.0000130800f,
    -0.0000709087f, -0.0000248566f, -0.0001133155f, -0.0000434898f, -0.0001730405f, -0.0000716494f,
    -0.0002548115f, -0.0001127059f, -0.0003641128f, -0.0001707647f, -0.0005072066f, -0.0002508449f,
    -0.0006911654f, -0.0003588874f, -0.0009238911f, -0.0005019296f, -0.0012141470f, -0.0006882036f,
    -0.0015715927f, -0.0009272484f, -0.0020068828f, -0.0012301269f, -0.0025317868f, -0.0016096577f,
    -0.0031594274f, -0.0020807422f, -0.0039046493f, -0.0026608782f, -0.0047846216f, -0.0033709154f,
    -0.0058197464f, -0.0042361832f, -0.0070351032f, -0.0052882686f, -0.0084627347f, -0.0065678174f,
    -0.0101452575f, -0.0081290805f, -0.0121418016f, -0.0100475023f, -0.0145381068f, -0.0124329648f,
    -0.0174644710f, -0.0154541276f, -0.0211296792f, -0.0193862546f, -0.0258903268f, -0.0247138070f,
    -0.0324076237f, -0.0323779736f, -0.0420549320f, -0.0444812371f, -0.0582141423f, -0.0668707061f,
    -0.0920161676f, -0.1241211681f, -0.2140038059f, -0.6341089063f,  0.6341089063f,  0.2140038059f,
     0.1241211681f,  0.0920161676f,  0.0668707061f,  0.0582141423f,  0.0444812371f,  0.0420549320f,
     0.0323779736f,  0.0324076237f,  0.0247138070f,  0.0258903268f,  0.0193862546f,  0.0211296792f,
     0.0154541276f,  0.0174644710f,  0.0124329648f,  0.0145381068f,  0.0100475023f,  0.0121418016f,
     0.0081290805f,  0.0101452575f,  0.0065678174f,  0.0084627347f,  0.0052882686f,  0.0070351032f,
     0.0042361832f,  0.0058197464f,  0.0033709154f,  0.0047846216f,  0.0026608782f,  0.0039046493f,
     0.0020807422f,  0.0031594274f,  0.0016096577f,  0.0025317868f,  0.0012301269f,  0.0020068828f,
     0.0009272484f,  0.0015715927f,  0.0006882036f,  0.0012141470f,  0.0005019296f,  0.0009238911f,
     0.0003588874f,  0.0006911654f,  0.0002508449f,  0.0005072066f,  0.0001707647f,  0.0003641128f,
     0.0001127059f,  0.0002548115f,  0.0000716494f,  0.0001730405f,  0.0000434898f,  0.0001133155f,
     0.0000248566f,  0.0000709087f,  0.0000130800f,  0.0000418038f,  0.0000060656f,  0.0000226577f,
     0.0000022259f,  0.0000130075f
};

LMSFilter* lms_filter_create(int M_taps, float mu_rate) {
    // Allocate memory for the main struct
    LMSFilter* filter = (LMSFilter*)malloc(sizeof(LMSFilter));
    if (filter == NULL) {
        return NULL; // Allocation failed
    }

    filter->M = M_taps;
    filter->mu = mu_rate;

    // Allocate memory for the internal buffers
    filter->g = (float*)malloc(sizeof(float) * M_taps);
    filter->x_buffer = (float*)malloc(sizeof(float) * M_taps);
    filter->anti_noise_buffer = (float*)malloc(sizeof(float) * M_taps);

    // Check if buffer allocations were successful
    if (filter->g == NULL || filter->x_buffer == NULL || filter->anti_noise_buffer == NULL) {
        // Free any successfully allocated memory before returning
        free(filter->g);
        free(filter->x_buffer);
        free(filter->anti_noise_buffer);
        free(filter);
        return NULL;
    }

    // Initialize the filter state
    lms_filter_init(filter);

    return filter;
}

void lms_filter_destroy(LMSFilter* filter) {
    if (filter != NULL) {
        free(filter->g);
        free(filter->x_buffer);
        free(filter->anti_noise_buffer);
        free(filter);
    }
}

void lms_filter_init(LMSFilter* filter) {
    if (filter == NULL) return;

    memset(filter->g, 0, sizeof(float) * filter->M);
    memset(filter->x_buffer, 0, sizeof(float) * filter->M);
    memset(filter->anti_noise_buffer, 0, sizeof(float) * filter->M);

    filter->e = 0.0f;
    filter->y_hat = 0.0f;
    filter->anti_noise = 0.0f;
    filter->anti_noise_90 = 0.0f;
    
    filter->head_x = 0;
    filter->head_anti = 0;
}

float lms_filter_process(LMSFilter* filter, float reference_input, float measured_signal) {
    if (filter == NULL) return 0.0f;
    
    // Store newest X sample in its circular buffer
    filter->x_buffer[filter->head_x] = reference_input;

    // --- LMS Filter Stage ---

    // 1. Compute estimated output (y_hat) using convolution
    filter->y_hat = 0.0f;
    int idx = filter->head_x;
    for (int j = 0; j < filter->M; j++) {
        filter->y_hat += filter->g[j] * filter->x_buffer[idx--];
        if (idx < 0) idx = filter->M - 1;
    }

    // 2. Compute error signal (e)
    filter->e = measured_signal - filter->y_hat;

    // 3. Update filter coefficients (g)
    idx = filter->head_x;
    for (int i = 0; i < filter->M; i++) {
        filter->g[i] += filter->mu * filter->e * filter->x_buffer[idx--];
        if (idx < 0) idx = filter->M - 1; // Corrected from idx < 1
    }

    // --- Hilbert Transform Stage ---

    // 1. Generate anti-noise signal
    filter->anti_noise = -filter->y_hat;
    
    // 2. Store in its circular buffer
    filter->anti_noise_buffer[filter->head_anti] = filter->anti_noise;

    // 3. Convolve with Hilbert FIR to get 90-degree shifted signal
    filter->anti_noise_90 = 0.0f;
    idx = filter->head_anti;
    for (int i = 0; i < filter->M; i++) {
        filter->anti_noise_90 += h_hilbert[i] * filter->anti_noise_buffer[idx--];
        if (idx < 0) idx = filter->M - 1;
    }
    
    // --- Advance circular buffer heads ---
    filter->head_x++;
    if (filter->head_x >= filter->M) filter->head_x = 0;

    filter->head_anti++;
    if (filter->head_anti >= filter->M) filter->head_anti = 0;
    
    // Note: The phase-shifted signal `filter->anti_noise_90` is calculated but not
    // used elsewhere in this function. You would typically output this to a DAC.
    
    // The primary output for cancellation is the error signal
    return filter->e;
}

