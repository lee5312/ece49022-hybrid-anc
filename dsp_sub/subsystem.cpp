/*

IMPORTANT:

- This is a REAL-TIME implementation unlike MATLAB simulation
- Processes ONE SAMPLE at a time
- Internally maintains history buffers

HOW TO USE:

1. Call once at startup:
   LMS_init();

2. For EVERY incoming sample:
   LMS_step(X_new, Y);

   where:
   X_new = current reference sample (noise reference)
   Y = current measured sample (mic signal = noise + speech)

OUTPUTS (updated every call):

y_hat → estimated noise component in Y
e → residual (Y - y_hat)
anti_noise → -y_hat
anti_noise_90 → 90° phase-shifted anti-noise (Hilbert)
g[] → learned FIR coefficients

IMPORTANT NOTES:

- Intended as a support module called by the main MCU file.
- Main program should provide microphone/audio samples.
- mu may need tuning on real hardware.
- M controls filter length / model size.
- A faster circular-buffer version is included below as
  commented code and may be used if optimization is needed.
- Hilbert FIR introduces delay ≈ (M-1)/2 samples
- X_new and Y must be synchronized sample-by-sample

*/

// LMS / Hilbert parameters
const int M = 128;
float mu = 0.005f;

// LMS variables
float g[M] = {0.0f};
float X[M] = {0.0f};

// Hilbert buffer for anti-noise
float anti_buf[M] = {0.0f};

// Outputs
float e = 0.0f;
float y_hat = 0.0f;
float anti_noise = 0.0f;
float anti_noise_90 = 0.0f;

// MATLAB-generated Hilbert coefficients
float h_hilbert[M] = {
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

// Initialize LMS variables
void LMS_init() {
    for (int i = 0; i < M; i++) {
        g[i] = 0.0f;
        X[i] = 0.0f;
        anti_buf[i] = 0.0f;
    }

    e = 0.0f;
    y_hat = 0.0f;
    anti_noise = 0.0f;
    anti_noise_90 = 0.0f;
}

// One LMS update step 
void LMS_step(float X_new, float Y) {

    // Shift old X samples
    for (int i = M - 1; i > 0; i--) {
        X[i] = X[i - 1];
    }
    X[0] = X_new;

    // Compute estimated output
    y_hat = 0.0f;
    for (int i = 0; i < M; i++) {
        y_hat += g[i] * X[i];
    }

    // Compute error
    e = Y - y_hat;

    // Update coefficients
    for (int i = 0; i < M; i++) {
        g[i] += mu * e * X[i];
    }

    // anti_noise = -y_hat
    anti_noise = -y_hat;

    // Shift anti-noise buffer
    for (int i = M - 1; i > 0; i--) {
        anti_buf[i] = anti_buf[i - 1];
    }
    anti_buf[0] = anti_noise;

    // anti_noise_90 = filter(h_hilbert, 1, anti_noise)
    anti_noise_90 = 0.0f;
    for (int i = 0; i < M; i++) {
        anti_noise_90 += h_hilbert[i] * anti_buf[i];
    }
}

/*
Optional Improved Version (Circular Buffer)

int head_x = 0;
int head_anti = 0;

void LMS_init_fast() {
    for (int i = 0; i < M; i++) {
        g[i] = 0.0f;
        X[i] = 0.0f;
        anti_buf[i] = 0.0f;
    }

    e = 0.0f;
    y_hat = 0.0f;
    anti_noise = 0.0f;
    anti_noise_90 = 0.0f;

    head_x = 0;
    head_anti = 0;
}

void LMS_step_fast(float X_new, float Y) {

    // Store newest X sample
    X[head_x] = X_new;

    // Compute estimated output
    y_hat = 0.0f;
    int idx = head_x;
    for (int i = 0; i < M; i++) {
        y_hat += g[i] * X[idx];
        idx--;
        if (idx < 0) idx = M - 1;
    }

    // Compute error
    e = Y - y_hat;

    // Update coefficients
    idx = head_x;
    for (int i = 0; i < M; i++) {
        g[i] += mu * e * X[idx];
        idx--;
        if (idx < 0) idx = M - 1;
    }

    // anti_noise = -y_hat
    anti_noise = -y_hat;

    // Store newest anti-noise sample
    anti_buf[head_anti] = anti_noise;

    // anti_noise_90 = Hilbert(anti_noise)
    anti_noise_90 = 0.0f;
    idx = head_anti;
    for (int i = 0; i < M; i++) {
        anti_noise_90 += h_hilbert[i] * anti_buf[idx];
        idx--;
        if (idx < 0) idx = M - 1;
    }

    // Advance circular heads
    head_x++;
    if (head_x >= M) head_x = 0;

    head_anti++;
    if (head_anti >= M) head_anti = 0;
}
*/
