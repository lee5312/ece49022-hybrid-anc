# Firmware Specification

## Active Firmware

The active firmware target is `firmware/teensy41`.

## Build Environments

| Environment | Purpose |
|-------------|---------|
| `teensy41_imu_only` | IMU-only bring-up |
| `teensy41_uwb_detect` | UWB detection without ranging |
| `teensy41_single_range` | single-link range validation |
| `teensy41_full_range` | full T/L/R range cycle |
| `teensy41_full_fusion` | IMU + UWB fusion path |

## Implemented Modules

- `imu.*` for SPI IMU sampling and calibration
- `dwm3000.*` for shared-bus multi-instance UWB access
- `uwb_ranging.*` for SS-TWR exchange logic
- `uwb_network.*` for multi-link UWB cycle management
- `eskf.*` and `matrix.*` for state estimation
- `main.cpp` for bring-up sequencing and telemetry

## Telemetry

The current firmware emits:

- `$POSE`
- `$UWBTS`
- `$UWBDBG`
- `$STAT`

When direct ADC microphone sampling is explicitly enabled for experiments, it can also emit:

- `$MICS`
- `$MICDBG`

## Current Limitations

- microphone/audio handling in the active integrated `Sheet1.NET` board is not fully implemented in firmware yet
- IMU mounting rotation still needs measured values
- UWB antenna delays still need measured values
- tool pose still depends on the local deployment geometry

## Legacy Firmware

- `firmware/esp32` and `firmware/stm32` are retained only as prototype history and are not the current deployment stack
