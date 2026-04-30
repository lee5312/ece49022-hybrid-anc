/**
 * matrix.c — Lightweight matrix math implementation
 *
 * Row-major float arrays.  No dynamic allocation.
 * All operations are O(n³) or less — suitable for real-time on RP2350.
 */

#include "matrix.h"
#include <math.h>
#include <string.h>

// ================================================================
//  Matrix operations
// ================================================================

void mat_mult(const float *A, const float *B, float *C,
              int m, int k, int n)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++)
                sum += A[i * k + p] * B[p * n + j];
            C[i * n + j] = sum;
        }
    }
}

void mat_mult_bt(const float *A, const float *B, float *C,
                 int m, int k, int n)
{
    // C[m×n] = A[m×k] * B[n×k]^T
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++)
                sum += A[i * k + p] * B[j * k + p];
            C[i * n + j] = sum;
        }
    }
}

void mat_at_mult(const float *A, const float *B, float *C,
                 int k, int m, int n)
{
    // C[m×n] = A[k×m]^T * B[k×n]
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++)
                sum += A[p * m + i] * B[p * n + j];
            C[i * n + j] = sum;
        }
    }
}

void mat_transpose(const float *A, float *B, int m, int n)
{
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            B[j * m + i] = A[i * n + j];
}

void mat_add(const float *A, const float *B, float *C, int m, int n)
{
    int sz = m * n;
    for (int i = 0; i < sz; i++)
        C[i] = A[i] + B[i];
}

void mat_sub(const float *A, const float *B, float *C, int m, int n)
{
    int sz = m * n;
    for (int i = 0; i < sz; i++)
        C[i] = A[i] - B[i];
}

void mat_scale(const float *A, float s, float *B, int m, int n)
{
    int sz = m * n;
    for (int i = 0; i < sz; i++)
        B[i] = A[i] * s;
}

void mat_add_scaled(float *A, const float *B, float s, int m, int n)
{
    int sz = m * n;
    for (int i = 0; i < sz; i++)
        A[i] += B[i] * s;
}

void mat_eye(float *A, int n)
{
    memset(A, 0, (size_t)(n * n) * sizeof(float));
    for (int i = 0; i < n; i++)
        A[i * n + i] = 1.0f;
}

void mat_zero(float *A, int m, int n)
{
    memset(A, 0, (size_t)(m * n) * sizeof(float));
}

void mat_copy(const float *src, float *dst, int m, int n)
{
    memcpy(dst, src, (size_t)(m * n) * sizeof(float));
}

// ================================================================
//  3-Vector operations
// ================================================================

float vec3_dot(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float vec3_norm(const float *v)
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void vec3_cross(const float *a, const float *b, float *c)
{
    c[0] = a[1] * b[2] - a[2] * b[1];
    c[1] = a[2] * b[0] - a[0] * b[2];
    c[2] = a[0] * b[1] - a[1] * b[0];
}

void skew3(const float *v, float *out)
{
    // Row-major 3×3
    out[0] =  0.0f;   out[1] = -v[2];   out[2] =  v[1];
    out[3] =  v[2];   out[4] =  0.0f;   out[5] = -v[0];
    out[6] = -v[1];   out[7] =  v[0];   out[8] =  0.0f;
}

// ================================================================
//  Quaternion operations  —  Hamilton convention [w, x, y, z]
// ================================================================

void quat_mult(const float *q1, const float *q2, float *out)
{
    float w1 = q1[0], x1 = q1[1], y1 = q1[2], z1 = q1[3];
    float w2 = q2[0], x2 = q2[1], y2 = q2[2], z2 = q2[3];

    out[0] = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2;
    out[1] = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2;
    out[2] = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2;
    out[3] = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2;
}

void quat_normalize(float *q)
{
    float n = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (n < 1e-12f) { q[0] = 1; q[1] = q[2] = q[3] = 0; return; }
    float inv = 1.0f / n;
    q[0] *= inv; q[1] *= inv; q[2] *= inv; q[3] *= inv;
}

void quat_conj(const float *q, float *out)
{
    out[0] =  q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] = -q[3];
}

// Rotation matrix R (body → world), 3×3 row-major
void quat_to_rotmat(const float *q, float *R)
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float xx = x*x, yy = y*y, zz = z*z;
    float xy = x*y, xz = x*z, yz = y*z;
    float wx = w*x, wy = w*y, wz = w*z;

    R[0] = 1 - 2*(yy + zz);  R[1] = 2*(xy - wz);      R[2] = 2*(xz + wy);
    R[3] = 2*(xy + wz);      R[4] = 1 - 2*(xx + zz);  R[5] = 2*(yz - wx);
    R[6] = 2*(xz - wy);      R[7] = 2*(yz + wx);      R[8] = 1 - 2*(xx + yy);
}

// Rotate vector v by quaternion q:  v' = R(q) * v
void quat_rotate(const float *q, const float *v, float *out)
{
    float R[9];
    quat_to_rotmat(q, R);
    out[0] = R[0]*v[0] + R[1]*v[1] + R[2]*v[2];
    out[1] = R[3]*v[0] + R[4]*v[1] + R[5]*v[2];
    out[2] = R[6]*v[0] + R[7]*v[1] + R[8]*v[2];
}

// Small rotation vector → quaternion (exact for any angle)
void rotvec_to_quat(const float *dtheta, float *dq)
{
    float angle = sqrtf(dtheta[0]*dtheta[0] +
                        dtheta[1]*dtheta[1] +
                        dtheta[2]*dtheta[2]);
    if (angle < 1e-8f) {
        dq[0] = 1.0f;
        dq[1] = 0.5f * dtheta[0];
        dq[2] = 0.5f * dtheta[1];
        dq[3] = 0.5f * dtheta[2];
    } else {
        float ha = 0.5f * angle;
        float s  = sinf(ha) / angle;
        dq[0] = cosf(ha);
        dq[1] = s * dtheta[0];
        dq[2] = s * dtheta[1];
        dq[3] = s * dtheta[2];
    }
    quat_normalize(dq);
}

// ZYX Tait-Bryan angles (in radians)
void quat_to_euler(const float *q, float *roll, float *pitch, float *yaw)
{
    float w = q[0], x = q[1], y = q[2], z = q[3];

    // Roll  (rotation about X)
    float sinr = 2.0f * (w * x + y * z);
    float cosr = 1.0f - 2.0f * (x * x + y * y);
    *roll = atan2f(sinr, cosr);

    // Pitch (rotation about Y) — clamp to avoid NaN near ±90°
    float sinp = 2.0f * (w * y - z * x);
    if (sinp >  1.0f) sinp =  1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    *pitch = asinf(sinp);

    // Yaw   (rotation about Z)
    float siny = 2.0f * (w * z + x * y);
    float cosy = 1.0f - 2.0f * (y * y + z * z);
    *yaw = atan2f(siny, cosy);
}
