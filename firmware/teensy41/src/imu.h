#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t timestamp_us;
    int16_t accel_raw[3];
    int16_t gyro_raw[3];
    float accel_g[3];
    float gyro_dps[3];
    bool valid;
} imu_sample_t;

// ── Initialization ──────────────────────────────────────────────
// Configures SPI and LSM6DS3TR-C registers.
// Returns true on success (WHO_AM_I confirmed).
bool imu_init(void);

// ── Raw data (LSB) ──────────────────────────────────────────────
void imu_get_accel_raw(int16_t *ax, int16_t *ay, int16_t *az);
void imu_get_gyro_raw(int16_t *gx, int16_t *gy, int16_t *gz);

// ── Converted data (g / dps) ────────────────────────────────────
void imu_get_accel_g(float *ax, float *ay, float *az);
void imu_get_gyro_dps(float *gx, float *gy, float *gz);

// ── Read latest sample from sensor ──────────────────────────────
// Call this at your desired rate (e.g. timer interrupt or loop).
// Reads both accel + gyro and updates the driver's cached sample.
void imu_read_all(void);

// Convenience helper that reads the sensor and returns a timestamped sample.
bool imu_read_sample(imu_sample_t *sample);

// ── Calibration ─────────────────────────────────────────────────
// Keep the board still and call this once after init.
// Averages `num_samples` readings to estimate gyro bias and accel offset.
// Bias values are automatically subtracted in imu_get_*_g / imu_get_*_dps.
void imu_calibrate(uint16_t num_samples);

// Get the stored bias values (useful for logging / debugging)
void imu_get_gyro_bias(float *bx, float *by, float *bz);
void imu_get_accel_bias(float *bx, float *by, float *bz);

#ifdef __cplusplus
}
#endif

#endif // IMU_H
