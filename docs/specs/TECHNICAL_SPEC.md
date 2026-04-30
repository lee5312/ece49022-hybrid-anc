# Technical Specification
## Spatially Aware Hybrid ANC System

**Version**: 2.0
**Status**: Active integrated-board baseline
**Primary hardware reference**: `ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`

---

## 1. System Summary

The current system is an integrated `Teensy 4.1` mainboard for headset sensing and audio interfacing, paired with three wired satellite boards and one mainboard UWB module.

At a high level, the system combines:

- UWB ranging for headset-to-tool spatial measurements
- IMU sensing for motion and orientation propagation
- microphone and audio front-end circuitry on the mainboard
- ANC simulation, calibration, and visualization tools in software

---

## 2. Active Hardware Architecture

### 2.1 Main Processing Board

| Item | Current Baseline |
|------|------------------|
| MCU | Teensy 4.1 |
| Mainboard UWB | `U16 = DWM3000TR13` |
| IMU Interface | SPI via `J_IMU_1` |
| UWB Satellite Links | 3 satellite headers: Tool, Left, Right |
| Audio Converters | `PCM1808 x2`, `PCM5102A x3` |
| Analog Audio Blocks | OPA4192, OPA2192, OPA1656, AD835 stages |

### 2.2 UWB Topology

| Node | Location | Role |
|------|----------|------|
| Mainboard UWB | headset top/mainboard | UWB participant on shared SPI bus |
| Tool satellite | tool-side remote board | UWB participant |
| Left satellite | left side remote board | UWB participant |
| Right satellite | right side remote board | UWB participant |

The firmware currently uses the three headset-side logical ranging links `T`, `L`, `R` against the Tool node for bench bring-up and fusion.

### 2.3 Satellite Board Interface

All three satellite boards share the same 9-pin connector structure:

- `3.3V`
- `GND`
- shared SPI clock
- shared SPI MOSI
- shared SPI MISO
- per-board `CS`
- per-board `RST`
- per-board `IRQ`
- one analog pin 9 path

Important distinction:

- Tool satellite analog pin 9 is filtered and routed to `IC1-17`
- Left and Right satellite analog pin 9 feed separate analog stages and do not go straight to Teensy ADC pins

---

## 3. Active Firmware Scope

The active firmware implementation is `firmware/teensy41`.

Implemented now:

- staged UWB bring-up modes
- IMU sampling and calibration hooks
- ESKF prediction/update flow
- serial telemetry for range and pose validation

Not yet complete:

- production audio codec/analog firmware path
- final calibration constants for UWB and IMU mounting

---

## 4. Performance Targets

| Function | Current Target |
|----------|----------------|
| IMU loop | `200 Hz` |
| UWB cycle | `5 Hz` |
| Telemetry | `20 Hz` |
| UWB bus clock | `2 MHz` |

---

## 5. Open Calibration Items

The following values are board- or setup-specific and remain required before deployment:

- `IMU_R_HI`
- UWB TX/RX antenna delays for all modules
- tool world-frame position
- audio-path calibration and H-map generation

---

## 6. Detailed References

- `HARDWARE_SPEC.md`
- `FIRMWARE_SPEC.md`
- `CALIBRATION.md`
- `ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`
- `SATELLITE_BOARD_KR.md`
