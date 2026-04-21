# Calibration Plan

## 1. Required Before Deployment

### IMU Mounting Rotation

Measure the rigid transform from IMU frame to headset body frame and replace the placeholder `IMU_R_HI` values in `firmware/teensy41/src/board_config.h`.

### UWB Antenna Delays

Measure and store per-module:

- top TX/RX antenna delay
- left TX/RX antenna delay
- right TX/RX antenna delay
- tool TX/RX antenna delay

### Tool World Position

Measure the Tool reference position for each bench or demo setup and replace the placeholder `TOOL_WX / TOOL_WY / TOOL_WZ`.

## 2. Bring-Up Sequence

1. Build `teensy41_imu_only`
2. Verify IMU communication and bias calibration
3. Build `teensy41_uwb_detect`
4. Verify all UWB modules report valid device IDs
5. Build `teensy41_single_range`
6. Validate one stable range link
7. Build `teensy41_full_range`
8. Validate all expected links
9. Build `teensy41_full_fusion`
10. Record pose and range telemetry for bench validation

## 3. Satellite Board Validation

Before full-system integration, validate each satellite board for:

- no power short
- stable DWM3000 device ID read
- valid reset and IRQ behavior
- microphone continuity on pin 9 path

## 4. Audio Path

The active `Sheet1.NET` mainboard includes integrated codec and analog blocks.
Final ANC calibration therefore requires additional work beyond the current Teensy sensor firmware:

- mapping the codec bus pins used by the integrated board
- defining the calibration microphone acquisition path
- generating and validating H-map data against the integrated analog chain

## 5. Current State

The repository currently contains:

- staged Teensy bring-up firmware
- visualization support for pose/range telemetry
- simulation and calibration scaffolding

It does not yet contain a finished production audio calibration workflow for the integrated board.
