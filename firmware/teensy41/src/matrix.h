/**
 * matrix.h — Lightweight fixed-size matrix math for ESKF
 *
 * All matrices stored as flat float arrays in ROW-MAJOR order.
 * Maximum dimension: ESKF uses 15×15 covariance matrix.
 * Optimized for clarity and small code size on RP2350.
 */

#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Matrix operations (row-major float arrays) ─────────────────

// C = A * B   where A is m×k, B is k×n, C is m×n
void mat_mult(const float *A, const float *B, float *C,
              int m, int k, int n);

// C = A * B^T  where A is m×k, B is n×k, C is m×n
void mat_mult_bt(const float *A, const float *B, float *C,
                 int m, int k, int n);

// C = A^T * B  where A is k×m, B is k×n, C is m×n
void mat_at_mult(const float *A, const float *B, float *C,
                 int k, int m, int n);

// B = A^T  where A is m×n, B is n×m
void mat_transpose(const float *A, float *B, int m, int n);

// C = A + B  (element-wise, size m×n)
void mat_add(const float *A, const float *B, float *C, int m, int n);

// C = A - B  (element-wise, size m×n)
void mat_sub(const float *A, const float *B, float *C, int m, int n);

// B = s * A  (scalar multiply, size m×n)
void mat_scale(const float *A, float s, float *B, int m, int n);

// A += s * B  (in-place accumulate, size m×n)
void mat_add_scaled(float *A, const float *B, float s, int m, int n);

// A = I_n  (set to identity)
void mat_eye(float *A, int n);

// A = 0   (set to zero, size m×n)
void mat_zero(float *A, int m, int n);

// dst = src  (copy, size m×n)
void mat_copy(const float *src, float *dst, int m, int n);

// ── Vector operations (length-3) ────────────────────────────────

float vec3_dot(const float *a, const float *b);
float vec3_norm(const float *v);
void  vec3_cross(const float *a, const float *b, float *c);

// ── Skew-symmetric matrix from 3-vector ─────────────────────────
//          [  0  -vz   vy ]
// [v]× =  [  vz   0  -vx ]
//          [ -vy  vx   0  ]
// out is 3×3 (9 floats)
void skew3(const float *v, float *out);

// ── Quaternion operations [w, x, y, z] convention ───────────────

void  quat_mult(const float *q1, const float *q2, float *out);
void  quat_normalize(float *q);
void  quat_conj(const float *q, float *out);
void  quat_to_rotmat(const float *q, float *R);   // 3×3
void  quat_rotate(const float *q, const float *v, float *out);
void  rotvec_to_quat(const float *dtheta, float *dq);

// ── Euler angle extraction (ZYX / Tait-Bryan) ──────────────────
void  quat_to_euler(const float *q, float *roll, float *pitch, float *yaw);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_H
