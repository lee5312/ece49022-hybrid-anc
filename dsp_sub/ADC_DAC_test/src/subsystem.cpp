/*

How to Use:

1. Call LMS_init() once during setup.

2. For each new incoming sample, call:

   LMS_step(X_new, Y);

   X_new = newest reference/input sample
   Y = newest measured/output sample

3. After each call:

   y_hat = current estimated output
   e = current error
   g[] = updated filter coefficients

Notes:
- Intended as a support module called by the main MCU file.
- Main program should provide microphone/audio samples.
- mu may need tuning on real hardware.
- M controls filter length / model size.
- A faster circular-buffer version is included below as
  commented code and may be used if optimization is needed.

*/

// LMS parameters
const int M = 128;
float mu = 0.005f;

// LMS variables
float g[M] = {0.0f};
float X[M] = {0.0f};

float e = 0.0f;
float y_hat = 0.0f;

// Initialize LMS variables
void LMS_init() {
    for (int i = 0; i < M; i++) {
        g[i] = 0.0f;
        X[i] = 0.0f;
    }

    e = 0.0f;
    y_hat = 0.0f;
}

// One LMS update step
void LMS_step(float X_new, float Y) {

    // Shift old samples
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
}

/*
Optional Improved Version (Circular Buffer)


int head = 0;

void LMS_init_fast() {
    for (int i = 0; i < M; i++) {
        g[i] = 0.0f;
        X[i] = 0.0f;
    }

    e = 0.0f;
    y_hat = 0.0f;
    head = 0;
}

void LMS_step_fast(float X_new, float Y) {

    X[head] = X_new;

    y_hat = 0.0f;

    int idx = head;
    for (int i = 0; i < M; i++) {
        y_hat += g[i] * X[idx];
        idx--;
        if (idx < 0) idx = M - 1;
    }

    e = Y - y_hat;

    idx = head;
    for (int i = 0; i < M; i++) {
        g[i] += mu * e * X[idx];
        idx--;
        if (idx < 0) idx = M - 1;
    }

    head++;
    if (head >= M) head = 0;
}
*/
