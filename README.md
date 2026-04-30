# Spatially Aware Hybrid Active Noise Cancelling

ECE 49022 senior design repository for the integrated `Teensy 4.1` mainboard, satellite UWB boards, and supporting ANC research code.

## Current Baseline

The active hardware target is no longer the earlier `ESP32 + STM32` split prototype.
The current repository baseline is:

- integrated `Teensy 4.1` mainboard
- mainboard UWB module plus `Tool / Left / Right` satellite UWB boards
- SPI IMU on `J_IMU_1`
- integrated audio ADC/DAC and analog processing blocks on the mainboard
- PlatformIO-based `teensy41` sensor firmware

Older `firmware/esp32` and `firmware/stm32` folders are kept only as legacy prototype history.

## Source Of Truth

The active hardware source of truth is:

- `docs/specs/ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`
- `docs/specs/SATELLITE_BOARD_KR.md`

Those documents supersede the older simplified handoff notes and the STM32-oriented PCB handoff document.

## Repository Structure

```text
ece49022-hybrid-anc/
|- docs/
|  |- reports/
|  `- specs/
|- firmware/
|  |- teensy41/         # Active Teensy 4.1 sensor firmware
|  |- esp32/            # Legacy prototype firmware
|  `- stm32/            # Legacy prototype firmware
|- hardware/
|  |- altium/           # Altium/netlist references for the active board
|  |- bom/
|  `- kicad/            # Legacy placeholder
|- software/
|  |- calibration/
|  `- visualization/
|- simulations/
`- tests/
```

## Active Firmware Build

```bash
cd firmware/teensy41
pio run -e teensy41_full_fusion
```

Available Teensy environments:

- `teensy41_imu_only`
- `teensy41_uwb_detect`
- `teensy41_single_range`
- `teensy41_full_range`
- `teensy41_full_fusion`

## Visualization

```bash
pip install -r software/visualization/requirements.txt
python software/visualization/visualizer.py
```

The current Teensy firmware emits bench telemetry such as `$POSE`, `$UWBTS`, `$UWBDBG`, and `$STAT`.

## Documentation

- [Technical Specification](docs/specs/TECHNICAL_SPEC.md)
- [Hardware Specification](docs/specs/HARDWARE_SPEC.md)
- [Firmware Specification](docs/specs/FIRMWARE_SPEC.md)
- [Calibration Plan](docs/specs/CALIBRATION.md)
- [Mainboard Netlist Reference](docs/specs/ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md)
- [Satellite Board Reference](docs/specs/SATELLITE_BOARD_KR.md)
- [Legacy STM32 PCB Handoff](docs/specs/PCB_HANDOFF_STM32.md)

## Current Status

Implemented and build-verified:

- Teensy 4.1 UWB bring-up and ranging stages
- IMU sampling and ESKF fusion scaffolding
- serial telemetry for bench validation
- repository-side documentation updates for the integrated board

Still pending for full deployment:

- measured `IMU_R_HI`
- per-module UWB antenna delay calibration
- measured tool world pose for each test setup
- final firmware mapping for the integrated audio codec/analog path

## Notes

- `simulations/` and `dsp_sub/` contain ongoing ANC modeling work and are not yet a complete deployable audio stack.
- The integrated mainboard contains substantially more audio hardware than the earlier simplified sensor-only documentation implied.
