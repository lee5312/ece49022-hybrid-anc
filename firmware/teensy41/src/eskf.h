/**
 * eskf.h — Error-State Kalman Filter for IMU + UWB fusion
 *
 * Reverse-trilateration architecture:
 *   - 3 UWB modules (T/L/R) on the headset at KNOWN body-frame positions
 *   - 1 UWB on the tool at a KNOWN world-frame position (fixed reference)
 *   - IMU on the headset for high-rate prediction
 *
 * State vector (nominal, 16 elements):
 *   p[3]     — headset position    (m, world frame)
 *   v[3]     — headset velocity    (m/s, world frame)
 *   q[4]     — headset orientation (quaternion [w,x,y,z], body→world)
 *   b_a[3]   — accel bias          (m/s²)
 *   b_g[3]   — gyro bias           (rad/s)
 *
 * Error-state vector (15 elements):
 *   δp[3], δv[3], δθ[3], δb_a[3], δb_g[3]
 *
 * Measurement model for UWB i:
 *   h_i = || p + R·b_i − t ||
 *   where b_i = body-frame UWB position, t = tool world position
 *
 * Jacobian H_i (1×15):
 *   ∂h/∂δp  = dᵢᵀ / rᵢ
 *   ∂h/∂δθ  = −(dᵢᵀ / rᵢ)·R·[bᵢ]×
 *   (others zero)
 *
 * Coordinate frame:  ENU (X=East, Y=North, Z=Up).
 * Gravity:           [0, 0, −9.80665] m/s².
 */

#ifndef ESKF_H
#define ESKF_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error-state dimension
#define ESKF_N          15

// State indices within the error vector
#define ESKF_IDX_P      0     // position [0..2]
#define ESKF_IDX_V      3     // velocity [3..5]
#define ESKF_IDX_TH     6     // attitude error [6..8]
#define ESKF_IDX_BA     9     // accel bias [9..11]
#define ESKF_IDX_BG     12    // gyro bias [12..14]

// Maximum number of UWB modules on headset
#define ESKF_MAX_UWB    3

// ================================================================
//  Noise parameters (tunable)
// ================================================================

typedef struct {
    float sigma_accel;      // Accel noise std (m/s²)       [~0.012]
    float sigma_gyro;       // Gyro noise std (rad/s)       [~0.001]
    float sigma_ba;         // Accel bias random walk        [~0.0004]
    float sigma_bg;         // Gyro bias random walk         [~0.00002]
    float sigma_uwb;        // UWB range noise std (m)       [~0.05]
    float mahal_gate;       // Mahalanobis gate threshold     [~3.0]
} eskf_noise_t;

// ================================================================
//  ESKF state structure
// ================================================================

typedef struct {
    // ── Nominal state ───────────────────────────────────────────
    float pos[3];           // headset position (m, world)
    float vel[3];           // headset velocity (m/s, world)
    float quat[4];          // headset orientation [w, x, y, z]
    float bias_accel[3];    // accelerometer bias (m/s²)
    float bias_gyro[3];     // gyroscope bias (rad/s)

    // ── Error-state covariance P (15×15) ────────────────────────
    float P[ESKF_N * ESKF_N];

    // ── UWB body-frame positions (headset frame) ────────────────
    float uwb_body_pos[ESKF_MAX_UWB][3];   // T, L, R in body frame

    // ── Tool world-frame position (fixed reference) ─────────────
    float tool_pos[3];

    // ── Noise config ────────────────────────────────────────────
    eskf_noise_t noise;

    // ── Bookkeeping ─────────────────────────────────────────────
    bool  initialized;
    float last_predict_time_s;
    uint32_t uwb_accepted;      // lifetime accepted count
    uint32_t uwb_rejected;      // lifetime rejected count
} eskf_state_t;

// ================================================================
//  API
// ================================================================

/**
 * Initialize ESKF with default noise parameters.
 * Sets orientation to identity (Z-up), zero velocity, origin position.
 */
void eskf_init(eskf_state_t *state);

/** Override noise parameters. */
void eskf_set_noise(eskf_state_t *state, const eskf_noise_t *noise);

/**
 * Set UWB body-frame position (position of UWB module on the headset).
 */
void eskf_set_uwb_body_pos(eskf_state_t *state, uint8_t idx,
                            float bx, float by, float bz);

/**
 * Set tool world-frame position (fixed reference point).
 */
void eskf_set_tool_pos(eskf_state_t *state, float tx, float ty, float tz);

/**
 * Prediction step using IMU measurements.
 */
void eskf_predict(eskf_state_t *state,
                  const float accel_ms2[3],
                  const float gyro_rads[3],
                  float dt);

/**
 * Update step using a single UWB range measurement.
 * Sequential scalar update — call once per headset UWB.
 *
 * Measurement model:  h = || p + R·b_i − tool_pos ||
 * Jacobian couples position AND attitude.
 *
 * @param state     ESKF state
 * @param uwb_idx   Which headset UWB (0=T, 1=L, 2=R)
 * @param range_m   Measured range to the tool (meters)
 * @return true if measurement was accepted, false if rejected (outlier)
 */
bool eskf_update_range(eskf_state_t *state, uint8_t uwb_idx, float range_m);

/**
 * After all sequential updates in a cycle, inject the accumulated
 * error into the nominal state and reset the error state.
 */
void eskf_inject_and_reset(eskf_state_t *state);

/**
 * Get current Euler angles (radians).
 */
void eskf_get_euler(const eskf_state_t *state,
                    float *roll, float *pitch, float *yaw);

#ifdef __cplusplus
}
#endif

#endif // ESKF_H
