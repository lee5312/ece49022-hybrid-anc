#include "lms_filter.h"
#include <cstring>

// MATLAB-generated Hilbert coefficients (static const, defined in the class)
const float LMSFilter::h_hilbert[128] = {
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

LMSFilter::LMSFilter(int M_taps, float mu_rate) : M(M_taps), mu(mu_rate) {
    g = new float[M];
    x_buffer = new float[M];
    anti_noise_buffer = new float[M];
    init(); // Initialize on creation
}

LMSFilter::~LMSFilter() {
    // Free the allocated memory
    delete[] g;
    delete[] x_buffer;
    delete[] anti_noise_buffer;
}

void LMSFilter::init() {
    memset(g, 0, sizeof(float) * M);
    memset(x_buffer, 0, sizeof(float) * M);
    memset(anti_noise_buffer, 0, sizeof(float) * M);
    e = 0.0f;
    y_hat = 0.0f;
    anti_noise = 0.0f;
    anti_noise_90 = 0.0f;
    head_x = 0;
    head_anti = 0;
}

float LMSFilter::process(float reference_input, float measured_signal) {
    // Store newest X sample
    x_buffer[head_x] = reference_input;

    // --- LMS Filter ---
    // 1. Compute estimated output (y_hat)
    y_hat = 0.0f;
    int idx = head_x;
    for (int i = 0; i < M; i++) {
        y_hat += g[i] * x_buffer[idx--];
        if (idx < 0) idx = M - 1;
    }

    // 2. Compute error signal (e)
    e = measured_signal - y_hat;

    // 3. Update filter coefficients (g)
    idx = head_x;
    for (int i = 0; i < M; i++) {
        g[i] += mu * e * x_buffer[idx--];
        if (idx < 1) idx = M - 1;
    }

    // --- Hilbert Transform Stage ---
    // 1. Generate anti-noise signal
    anti_noise = -y_hat;
    
    // 2. Store in its circular buffer
    anti_noise_buffer[head_anti] = anti_noise;

    // 3. Convolve with Hilbert FIR to get 90-degree shifted signal
    anti_noise_90 = 0.0f;
    idx = head_anti;
    for (int i = 0; i < M; i++) {
        anti_noise_90 += h_hilbert[i] * anti_noise_buffer[idx--];
        if (idx < 0) idx = M - 1;
    }
    
    // --- Advance circular buffer heads ---
    head_x++;
    if (head_x >= M) head_x = 0;

    head_anti++;
    if (head_anti >= M) head_anti = 0;

    // The primary output for cancellation is the error signal
    return e;
}
