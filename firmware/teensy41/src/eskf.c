/**
 * eskf.c — Error-State Kalman Filter implementation
 *
 * Reverse-trilateration measurement model:
 *   h_i = || p + R·b_i − t ||
 *   where b_i = body-frame UWB position, t = tool world position
 *
 * Jacobian H_i (1×15):
 *   ∂h/∂δp  = dᵢᵀ / rᵢ             (position, indices 0-2)
 *   ∂h/∂δv  = 0                      (velocity, indices 3-5)
 *   ∂h/∂δθ  = −(dᵢᵀ / rᵢ)·R·[bᵢ]×  (attitude, indices 6-8)
 *   ∂h/∂δba = 0                      (accel bias, indices 9-11)
 *   ∂h/∂δbg = 0                      (gyro bias, indices 12-14)
 *
 * Conventions:
 *   - Quaternion: Hamilton, [w, x, y, z], body → world rotation
 *   - Frame: ENU (Z-up), gravity = [0, 0, −9.80665] m/s²
 *   - Matrices: row-major flat float arrays
 *
 * ESKF cycle:
 *   1. eskf_predict()          — called every IMU sample (~200 Hz)
 *   2. eskf_update_range() ×3  — called when UWB cycle completes
 *   3. eskf_inject_and_reset() — inject error, reset δx
 */

#include "eskf.h"
#include "matrix.h"

#include <math.h>
#include <string.h>

#define GRAVITY  9.80665f
#define N        ESKF_N    // 15

// Scratch buffers (static to avoid stack overflow on RP2350)
static float s_F[N * N];       // Error-state transition
static float s_Phi[N * N];     // Discretized Phi = I + F*dt
static float s_PhiT[N * N];    // Phi transpose
static float s_tmp[N * N];     // Temporary
static float s_R3[9];          // 3×3 rotation matrix

// Error-state accumulator (between inject_and_reset calls)
static float s_dx[N];

// ================================================================
//  Initialization
// ================================================================

void eskf_init(eskf_state_t *state)
{
    memset(state, 0, sizeof(eskf_state_t));

    // Identity quaternion (Z-up)
    state->quat[0] = 1.0f;

    // Initial covariance — moderate uncertainty
    mat_eye(state->P, N);
    // Position: 1 m² uncertainty
    state->P[0*N+0] = 1.0f;
    state->P[1*N+1] = 1.0f;
    state->P[2*N+2] = 1.0f;
    // Velocity: 0.1 (m/s)²
    state->P[3*N+3] = 0.1f;
    state->P[4*N+4] = 0.1f;
    state->P[5*N+5] = 0.1f;
    // Attitude: (5°)² ≈ 0.0076 rad²
    state->P[6*N+6] = 0.0076f;
    state->P[7*N+7] = 0.0076f;
    state->P[8*N+8] = 0.0076f;
    // Accel bias: (0.05 m/s²)²
    state->P[9*N+9]   = 0.0025f;
    state->P[10*N+10] = 0.0025f;
    state->P[11*N+11] = 0.0025f;
    // Gyro bias: (0.01 rad/s)²
    state->P[12*N+12] = 0.0001f;
    state->P[13*N+13] = 0.0001f;
    state->P[14*N+14] = 0.0001f;

    // Default noise parameters
    state->noise.sigma_accel = 0.012f;
    state->noise.sigma_gyro  = 0.001f;
    state->noise.sigma_ba    = 0.0004f;
    state->noise.sigma_bg    = 0.00002f;
    state->noise.sigma_uwb   = 0.05f;
    state->noise.mahal_gate   = 3.0f;

    state->uwb_accepted = 0;
    state->uwb_rejected = 0;

    memset(s_dx, 0, sizeof(s_dx));

    state->initialized = true;
}

void eskf_set_noise(eskf_state_t *state, const eskf_noise_t *noise)
{
    state->noise = *noise;
}

void eskf_set_uwb_body_pos(eskf_state_t *state, uint8_t idx,
                            float bx, float by, float bz)
{
    if (idx >= ESKF_MAX_UWB) return;
    state->uwb_body_pos[idx][0] = bx;
    state->uwb_body_pos[idx][1] = by;
    state->uwb_body_pos[idx][2] = bz;
}

void eskf_set_tool_pos(eskf_state_t *state, float tx, float ty, float tz)
{
    state->tool_pos[0] = tx;
    state->tool_pos[1] = ty;
    state->tool_pos[2] = tz;
}

// ================================================================
//  Prediction step
// ================================================================

void eskf_predict(eskf_state_t *state,
                  const float accel_ms2[3],
                  const float gyro_rads[3],
                  float dt)
{
    if (!state->initialized || dt <= 0.0f) return;

    float *p  = state->pos;
    float *v  = state->vel;
    float *q  = state->quat;
    float *ba = state->bias_accel;
    float *bg = state->bias_gyro;

    // ── Corrected measurements ──────────────────────────────────
    float a_body[3] = {
        accel_ms2[0] - ba[0],
        accel_ms2[1] - ba[1],
        accel_ms2[2] - ba[2]
    };
    float w_body[3] = {
        gyro_rads[0] - bg[0],
        gyro_rads[1] - bg[1],
        gyro_rads[2] - bg[2]
    };

    // ── Rotation matrix R (body → world) ────────────────────────
    quat_to_rotmat(q, s_R3);

    // ── Rotate acceleration to world frame ──────────────────────
    float a_world[3];
    a_world[0] = s_R3[0]*a_body[0] + s_R3[1]*a_body[1] + s_R3[2]*a_body[2];
    a_world[1] = s_R3[3]*a_body[0] + s_R3[4]*a_body[1] + s_R3[5]*a_body[2];
    a_world[2] = s_R3[6]*a_body[0] + s_R3[7]*a_body[1] + s_R3[8]*a_body[2];

    // ── Update nominal state (integrate) ────────────────────────
    // Position: p += v * dt + 0.5 * a_net * dt²
    float a_net[3] = { a_world[0], a_world[1], a_world[2] - GRAVITY };
    float dt2 = 0.5f * dt * dt;
    p[0] += v[0] * dt + a_net[0] * dt2;
    p[1] += v[1] * dt + a_net[1] * dt2;
    p[2] += v[2] * dt + a_net[2] * dt2;

    // Velocity: v += a_net * dt
    v[0] += a_net[0] * dt;
    v[1] += a_net[1] * dt;
    v[2] += a_net[2] * dt;

    // Quaternion: q = q ⊗ dq(ω * dt)
    float w_dt[3] = { w_body[0] * dt, w_body[1] * dt, w_body[2] * dt };
    float dq[4];
    rotvec_to_quat(w_dt, dq);
    float q_new[4];
    quat_mult(q, dq, q_new);
    quat_normalize(q_new);
    q[0] = q_new[0]; q[1] = q_new[1]; q[2] = q_new[2]; q[3] = q_new[3];

    // Biases: constant (zero derivative)

    // ── Build error-state transition F (15×15) ──────────────────
    //
    // F = [ 0₃  I₃    0₃      0₃    0₃  ]     δp
    //     [ 0₃  0₃  -R[a]×   -R     0₃  ]     δv
    //     [ 0₃  0₃  -[ω]×    0₃    -I₃  ]     δθ
    //     [ 0₃  0₃    0₃      0₃    0₃  ]     δba
    //     [ 0₃  0₃    0₃      0₃    0₃  ]     δbg

    mat_zero(s_F, N, N);

    // F[0:3, 3:6] = I₃
    s_F[0*N+3] = 1.0f;  s_F[1*N+4] = 1.0f;  s_F[2*N+5] = 1.0f;

    // F[3:6, 6:9] = -R * [a_body]×
    float a_skew[9];
    skew3(a_body, a_skew);

    // -R * [a]×
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float val = 0;
            for (int k = 0; k < 3; k++)
                val += s_R3[i*3+k] * a_skew[k*3+j];
            s_F[(3+i)*N + (6+j)] = -val;
        }

    // F[3:6, 9:12] = -R
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            s_F[(3+i)*N + (9+j)] = -s_R3[i*3+j];

    // F[6:9, 6:9] = -[ω]×
    float w_skew[9];
    skew3(w_body, w_skew);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            s_F[(6+i)*N + (6+j)] = -w_skew[i*3+j];

    // F[6:9, 12:15] = -I₃
    s_F[6*N+12] = -1.0f;  s_F[7*N+13] = -1.0f;  s_F[8*N+14] = -1.0f;

    // ── Discretize: Φ = I + F·dt ────────────────────────────────
    mat_eye(s_Phi, N);
    mat_add_scaled(s_Phi, s_F, dt, N, N);

    // ── Process noise Q_d (diagonal) ────────────────────────────
    //    Position: 0 (driven by velocity)
    //    Velocity: σ_a² · dt
    //    Attitude: σ_g² · dt
    //    Bias accel: σ_ba² · dt
    //    Bias gyro:  σ_bg² · dt
    float sa2 = state->noise.sigma_accel * state->noise.sigma_accel * dt;
    float sg2 = state->noise.sigma_gyro  * state->noise.sigma_gyro  * dt;
    float sba2 = state->noise.sigma_ba * state->noise.sigma_ba * dt;
    float sbg2 = state->noise.sigma_bg * state->noise.sigma_bg * dt;

    // ── Covariance prediction: P = Φ·P·Φᵀ + Q_d ────────────────
    mat_mult(s_Phi, state->P, s_tmp, N, N, N);      // tmp = Φ·P
    mat_transpose(s_Phi, s_PhiT, N, N);               // PhiT = Φᵀ
    mat_mult(s_tmp, s_PhiT, state->P, N, N, N);       // P = tmp·Φᵀ

    // Add diagonal noise
    for (int i = 3;  i < 6;  i++) state->P[i*N+i] += sa2;
    for (int i = 6;  i < 9;  i++) state->P[i*N+i] += sg2;
    for (int i = 9;  i < 12; i++) state->P[i*N+i] += sba2;
    for (int i = 12; i < 15; i++) state->P[i*N+i] += sbg2;

    // ── Symmetrize P (numerical stability) ──────────────────────
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            float avg = 0.5f * (state->P[i*N+j] + state->P[j*N+i]);
            state->P[i*N+j] = avg;
            state->P[j*N+i] = avg;
        }
}

// ================================================================
//  Update step  (sequential scalar update per range measurement)
//
//  Measurement model (reverse trilateration):
//    h_i = || p + R·b_i − t ||
//    where p = headset pos,  R = body→world rotation,
//          b_i = body-frame UWB pos,  t = tool world pos
//
//  Jacobian H (1×15):
//    H[0:3]  = dᵢᵀ / rᵢ              (∂h/∂δp)
//    H[3:6]  = 0                       (∂h/∂δv)
//    H[6:9]  = −(dᵢᵀ/rᵢ)·R·[bᵢ]×    (∂h/∂δθ)
//    H[9:15] = 0                       (∂h/∂δba, ∂h/∂δbg)
// ================================================================

bool eskf_update_range(eskf_state_t *state, uint8_t uwb_idx, float range_m)
{
    if (!state->initialized || uwb_idx >= ESKF_MAX_UWB) return false;

    float *p  = state->pos;
    float *b  = state->uwb_body_pos[uwb_idx];
    float *t  = state->tool_pos;

    // ── Current rotation matrix R (body → world) ────────────────
    float R[9];
    quat_to_rotmat(state->quat, R);

    // ── World-frame UWB position: w_i = p + R·b_i ──────────────
    float Rb[3];
    Rb[0] = R[0]*b[0] + R[1]*b[1] + R[2]*b[2];
    Rb[1] = R[3]*b[0] + R[4]*b[1] + R[5]*b[2];
    Rb[2] = R[6]*b[0] + R[7]*b[1] + R[8]*b[2];

    // ── Difference vector: d_i = (p + R·b_i) − tool_pos ────────
    float dx = p[0] + Rb[0] - t[0];
    float dy = p[1] + Rb[1] - t[1];
    float dz = p[2] + Rb[2] - t[2];

    // ── Predicted range ─────────────────────────────────────────
    float pred_range = sqrtf(dx*dx + dy*dy + dz*dz);
    if (pred_range < 0.01f) pred_range = 0.01f;

    // ── Innovation ──────────────────────────────────────────────
    float y = range_m - pred_range;

    // ── Measurement Jacobian H (1×15) ───────────────────────────
    float H[N];
    memset(H, 0, sizeof(H));

    float inv_r = 1.0f / pred_range;
    float d_hat[3] = { dx * inv_r, dy * inv_r, dz * inv_r };

    // ∂h/∂δp = dᵢᵀ / rᵢ
    H[0] = d_hat[0];
    H[1] = d_hat[1];
    H[2] = d_hat[2];

    // ∂h/∂δθ = −(dᵢᵀ/rᵢ) · R · [bᵢ]×
    //
    // [b]× = |  0  -bz  by |
    //        |  bz  0  -bx |
    //        | -by  bx  0  |
    //
    // R·[b]× is a 3×3 matrix.  We need dᵀ·(R·[b]×) which is 1×3.
    float b_skew[9];
    skew3(b, b_skew);

    // Compute R·[b]× (3×3)
    float Rb_skew[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float val = 0.0f;
            for (int k = 0; k < 3; k++)
                val += R[i*3+k] * b_skew[k*3+j];
            Rb_skew[i*3+j] = val;
        }

    // H[6:9] = −dᵀ · (R·[b]×)   (1×3 row vector)
    for (int j = 0; j < 3; j++) {
        float val = 0.0f;
        for (int i = 0; i < 3; i++)
            val += d_hat[i] * Rb_skew[i*3+j];
        H[6 + j] = -val;
    }

    // ── Innovation variance S = H·P·Hᵀ + R ─────────────────────
    //    Since H is 1×15, S is scalar: S = Σ Σ H[i]*P[i][j]*H[j] + R
    float S = 0.0f;
    for (int i = 0; i < N; i++) {
        float PH_i = 0.0f;
        for (int j = 0; j < N; j++)
            PH_i += state->P[i*N+j] * H[j];
        S += H[i] * PH_i;
    }
    S += state->noise.sigma_uwb * state->noise.sigma_uwb;

    if (S < 1e-10f) return false;   // degenerate, skip update

    // ── Mahalanobis distance outlier rejection ────────────────
    //    d_M = |y| / sqrt(S);  reject if d_M > gate threshold
    float mahal_sq = (y * y) / S;
    float gate = state->noise.mahal_gate;
    if (mahal_sq > gate * gate) {
        state->uwb_rejected++;
        return false;   // outlier — skip this measurement
    }

    float S_inv = 1.0f / S;

    // ── Kalman gain K = P·Hᵀ / S  (15×1) ───────────────────────
    float K[N];
    for (int i = 0; i < N; i++) {
        float PH_i = 0.0f;
        for (int j = 0; j < N; j++)
            PH_i += state->P[i*N+j] * H[j];
        K[i] = PH_i * S_inv;
    }

    // ── Error-state update: δx += K · y ─────────────────────────
    for (int i = 0; i < N; i++)
        s_dx[i] += K[i] * y;

    // ── Covariance update: P -= K · H · P (Joseph form simplified)
    //    P = P - K·(H·P)  ⟹  P[i][j] -= K[i] * (H·P)_j
    float HP[N];   // (H·P) = 1×15
    for (int j = 0; j < N; j++) {
        float val = 0.0f;
        for (int k = 0; k < N; k++)
            val += H[k] * state->P[k*N+j];
        HP[j] = val;
    }

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            state->P[i*N+j] -= K[i] * HP[j];

    // Symmetrize
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++) {
            float avg = 0.5f * (state->P[i*N+j] + state->P[j*N+i]);
            state->P[i*N+j] = avg;
            state->P[j*N+i] = avg;
        }

    state->uwb_accepted++;
    return true;
}

// ================================================================
//  Error injection + reset
// ================================================================

void eskf_inject_and_reset(eskf_state_t *state)
{
    // ── Inject position + velocity ──────────────────────────────
    state->pos[0] += s_dx[ESKF_IDX_P + 0];
    state->pos[1] += s_dx[ESKF_IDX_P + 1];
    state->pos[2] += s_dx[ESKF_IDX_P + 2];

    state->vel[0] += s_dx[ESKF_IDX_V + 0];
    state->vel[1] += s_dx[ESKF_IDX_V + 1];
    state->vel[2] += s_dx[ESKF_IDX_V + 2];

    // ── Inject attitude (small-angle quaternion) ────────────────
    float dtheta[3] = {
        s_dx[ESKF_IDX_TH + 0],
        s_dx[ESKF_IDX_TH + 1],
        s_dx[ESKF_IDX_TH + 2]
    };
    float dq[4];
    rotvec_to_quat(dtheta, dq);

    float q_new[4];
    quat_mult(state->quat, dq, q_new);
    quat_normalize(q_new);
    state->quat[0] = q_new[0];
    state->quat[1] = q_new[1];
    state->quat[2] = q_new[2];
    state->quat[3] = q_new[3];

    // ── Inject biases ───────────────────────────────────────────
    state->bias_accel[0] += s_dx[ESKF_IDX_BA + 0];
    state->bias_accel[1] += s_dx[ESKF_IDX_BA + 1];
    state->bias_accel[2] += s_dx[ESKF_IDX_BA + 2];

    state->bias_gyro[0] += s_dx[ESKF_IDX_BG + 0];
    state->bias_gyro[1] += s_dx[ESKF_IDX_BG + 1];
    state->bias_gyro[2] += s_dx[ESKF_IDX_BG + 2];

    // ── Reset error state ───────────────────────────────────────
    memset(s_dx, 0, sizeof(s_dx));
}

// ================================================================
//  Euler angles
// ================================================================

void eskf_get_euler(const eskf_state_t *state,
                    float *roll, float *pitch, float *yaw)
{
    quat_to_euler(state->quat, roll, pitch, yaw);
}
