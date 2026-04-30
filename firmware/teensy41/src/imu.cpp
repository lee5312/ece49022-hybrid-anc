#include "imu.h"

#include "board_config.h"

#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr uint32_t IMU_SPI_BAUD = 4UL * 1000UL * 1000UL;
const SPISettings kImuSpiSettings(IMU_SPI_BAUD, MSBFIRST, SPI_MODE3);

constexpr uint8_t REG_WHO_AM_I   = 0x0F;
constexpr uint8_t REG_CTRL1_XL   = 0x10;
constexpr uint8_t REG_CTRL2_G    = 0x11;
constexpr uint8_t REG_CTRL3_C    = 0x12;
constexpr uint8_t REG_OUTX_L_G   = 0x22;
constexpr uint8_t REG_OUTX_L_XL  = 0x28;
constexpr uint8_t WHO_AM_I_VALUE = 0x6A;

constexpr float ACCEL_SENSITIVITY_2G    = 0.000061f;
constexpr float GYRO_SENSITIVITY_245DPS = 0.00875f;

bool s_initialized = false;
bool s_spi_initialized = false;

int16_t s_raw_ax = 0;
int16_t s_raw_ay = 0;
int16_t s_raw_az = 0;
int16_t s_raw_gx = 0;
int16_t s_raw_gy = 0;
int16_t s_raw_gz = 0;

float s_bias_ax = 0.0f;
float s_bias_ay = 0.0f;
float s_bias_az = 0.0f;
float s_bias_gx = 0.0f;
float s_bias_gy = 0.0f;
float s_bias_gz = 0.0f;

void cs_select(void)
{
    digitalWrite(IMU_PIN_CS, LOW);
}

void cs_deselect(void)
{
    digitalWrite(IMU_PIN_CS, HIGH);
}

void imu_spi_init_pins(void)
{
    SPI.setMOSI(IMU_PIN_MOSI);
    SPI.setMISO(IMU_PIN_MISO);
    SPI.setSCK(IMU_PIN_SCK);
    SPI.begin();

    pinMode(IMU_PIN_CS, OUTPUT);
    digitalWrite(IMU_PIN_CS, HIGH);
}

void write_reg(uint8_t reg, uint8_t value)
{
    SPI.beginTransaction(kImuSpiSettings);
    cs_select();
    SPI.transfer(reg & 0x7F);
    SPI.transfer(value);
    cs_deselect();
    SPI.endTransaction();
}

uint8_t read_reg(uint8_t reg)
{
    SPI.beginTransaction(kImuSpiSettings);
    cs_select();
    SPI.transfer(reg | 0x80);
    uint8_t value = SPI.transfer(0xFF);
    cs_deselect();
    SPI.endTransaction();
    return value;
}

void read_burst(uint8_t start_reg, uint8_t *buf, size_t len)
{
    SPI.beginTransaction(kImuSpiSettings);
    cs_select();
    SPI.transfer(start_reg | 0x80);
    for (size_t i = 0; i < len; i++) {
        buf[i] = SPI.transfer(0xFF);
    }
    cs_deselect();
    SPI.endTransaction();
}

} // namespace

bool imu_init(void)
{
    if (!s_spi_initialized) {
        imu_spi_init_pins();
        s_spi_initialized = true;
    }

    delay(20);

    uint8_t who = read_reg(REG_WHO_AM_I);
    if (who != WHO_AM_I_VALUE) {
        return false;
    }

    write_reg(REG_CTRL3_C, static_cast<uint8_t>((1u << 6) | (1u << 2)));
    write_reg(REG_CTRL1_XL, 0x50);
    write_reg(REG_CTRL2_G, 0x50);

    s_raw_ax = s_raw_ay = s_raw_az = 0;
    s_raw_gx = s_raw_gy = s_raw_gz = 0;
    s_bias_ax = s_bias_ay = s_bias_az = 0.0f;
    s_bias_gx = s_bias_gy = s_bias_gz = 0.0f;
    s_initialized = true;
    return true;
}

void imu_read_all(void)
{
    if (!s_initialized) {
        return;
    }

    uint8_t buf[12];
    read_burst(REG_OUTX_L_G, buf, sizeof(buf));

    s_raw_gx = static_cast<int16_t>((buf[1] << 8) | buf[0]);
    s_raw_gy = static_cast<int16_t>((buf[3] << 8) | buf[2]);
    s_raw_gz = static_cast<int16_t>((buf[5] << 8) | buf[4]);
    s_raw_ax = static_cast<int16_t>((buf[7] << 8) | buf[6]);
    s_raw_ay = static_cast<int16_t>((buf[9] << 8) | buf[8]);
    s_raw_az = static_cast<int16_t>((buf[11] << 8) | buf[10]);
}

bool imu_read_sample(imu_sample_t *sample)
{
    if (!sample || !s_initialized) {
        return false;
    }

    imu_read_all();

    sample->timestamp_us = micros();
    sample->accel_raw[0] = s_raw_ax;
    sample->accel_raw[1] = s_raw_ay;
    sample->accel_raw[2] = s_raw_az;
    sample->gyro_raw[0] = s_raw_gx;
    sample->gyro_raw[1] = s_raw_gy;
    sample->gyro_raw[2] = s_raw_gz;
    sample->accel_g[0] = static_cast<float>(s_raw_ax) * ACCEL_SENSITIVITY_2G - s_bias_ax;
    sample->accel_g[1] = static_cast<float>(s_raw_ay) * ACCEL_SENSITIVITY_2G - s_bias_ay;
    sample->accel_g[2] = static_cast<float>(s_raw_az) * ACCEL_SENSITIVITY_2G - s_bias_az;
    sample->gyro_dps[0] = static_cast<float>(s_raw_gx) * GYRO_SENSITIVITY_245DPS - s_bias_gx;
    sample->gyro_dps[1] = static_cast<float>(s_raw_gy) * GYRO_SENSITIVITY_245DPS - s_bias_gy;
    sample->gyro_dps[2] = static_cast<float>(s_raw_gz) * GYRO_SENSITIVITY_245DPS - s_bias_gz;
    sample->valid = true;
    return true;
}

void imu_get_accel_raw(int16_t *ax, int16_t *ay, int16_t *az)
{
    if (ax) *ax = s_raw_ax;
    if (ay) *ay = s_raw_ay;
    if (az) *az = s_raw_az;
}

void imu_get_gyro_raw(int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (gx) *gx = s_raw_gx;
    if (gy) *gy = s_raw_gy;
    if (gz) *gz = s_raw_gz;
}

void imu_get_accel_g(float *ax, float *ay, float *az)
{
    if (ax) *ax = static_cast<float>(s_raw_ax) * ACCEL_SENSITIVITY_2G - s_bias_ax;
    if (ay) *ay = static_cast<float>(s_raw_ay) * ACCEL_SENSITIVITY_2G - s_bias_ay;
    if (az) *az = static_cast<float>(s_raw_az) * ACCEL_SENSITIVITY_2G - s_bias_az;
}

void imu_get_gyro_dps(float *gx, float *gy, float *gz)
{
    if (gx) *gx = static_cast<float>(s_raw_gx) * GYRO_SENSITIVITY_245DPS - s_bias_gx;
    if (gy) *gy = static_cast<float>(s_raw_gy) * GYRO_SENSITIVITY_245DPS - s_bias_gy;
    if (gz) *gz = static_cast<float>(s_raw_gz) * GYRO_SENSITIVITY_245DPS - s_bias_gz;
}

void imu_calibrate(uint16_t num_samples)
{
    if (!s_initialized || num_samples == 0) {
        return;
    }

    float sum_ax = 0.0f;
    float sum_ay = 0.0f;
    float sum_az = 0.0f;
    float sum_gx = 0.0f;
    float sum_gy = 0.0f;
    float sum_gz = 0.0f;

    for (uint16_t i = 0; i < num_samples; i++) {
        imu_read_all();

        sum_ax += static_cast<float>(s_raw_ax) * ACCEL_SENSITIVITY_2G;
        sum_ay += static_cast<float>(s_raw_ay) * ACCEL_SENSITIVITY_2G;
        sum_az += static_cast<float>(s_raw_az) * ACCEL_SENSITIVITY_2G;
        sum_gx += static_cast<float>(s_raw_gx) * GYRO_SENSITIVITY_245DPS;
        sum_gy += static_cast<float>(s_raw_gy) * GYRO_SENSITIVITY_245DPS;
        sum_gz += static_cast<float>(s_raw_gz) * GYRO_SENSITIVITY_245DPS;

        delay(IMU_LOOP_PERIOD_MS);
    }

    const float n = static_cast<float>(num_samples);
    s_bias_gx = sum_gx / n;
    s_bias_gy = sum_gy / n;
    s_bias_gz = sum_gz / n;
    s_bias_ax = sum_ax / n;
    s_bias_ay = sum_ay / n;
    s_bias_az = sum_az / n - 1.0f;
}

void imu_get_gyro_bias(float *bx, float *by, float *bz)
{
    if (bx) *bx = s_bias_gx;
    if (by) *by = s_bias_gy;
    if (bz) *bz = s_bias_gz;
}

void imu_get_accel_bias(float *bx, float *by, float *bz)
{
    if (bx) *bx = s_bias_ax;
    if (by) *by = s_bias_ay;
    if (bz) *bz = s_bias_az;
}
