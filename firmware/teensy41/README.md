# Teensy 4.1 Sensor Firmware

This folder contains the active mainboard firmware for the integrated `Teensy 4.1` design.

## Scope

- IMU bring-up on the `J_IMU_1` SPI wiring
- Mainboard UWB + 3 satellite UWB control on the shared SPI bus
- Single-range and full-range UWB bring-up modes
- ESKF-based UWB/IMU fusion
- Serial telemetry for bench validation and visualization

## Build Environments

- `teensy41_imu_only`
- `teensy41_uwb_detect`
- `teensy41_single_range`
- `teensy41_full_range`
- `teensy41_full_fusion`

Example:

```bash
cd firmware/teensy41
pio run -e teensy41_full_fusion
```

## Source Of Truth

The active hardware reference is:

- `docs/specs/ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`
- `docs/specs/SATELLITE_BOARD_KR.md`

Those documents supersede older STM32-oriented and simplified handoff notes.

## Current Limitations

- `IMU_R_HI` is still a placeholder identity rotation and must be replaced with measured mounting data.
- UWB antenna delay values are still placeholders and must be calibrated per module.
- Tool world coordinates are deployment-specific and must be measured.
- The integrated `Sheet1.NET` board does not currently match the legacy direct-ADC microphone assumptions.
  The timer-driven mic sampler is therefore disabled by default until the codec/analog path is mapped.

## Telemetry

The firmware currently emits bench-oriented serial records such as:

- `$POSE`
- `$UWBTS`
- `$UWBDBG`
- `$STAT`
- `$MICS` and `$MICDBG` only when `ENABLE_MIC_ADC=1`
