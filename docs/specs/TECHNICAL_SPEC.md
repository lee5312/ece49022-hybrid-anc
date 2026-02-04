# Technical Specification
## Spatially Aware Hybrid Active Noise Cancelling System

**Version**: 1.0  
**Date**: February 2026  
**Status**: Draft

---

## 1. System Overview

### 1.1 Purpose
The SAHANC system provides adaptive noise cancellation for industrial environments by combining:
- **Analog circuits** for low-latency high-frequency response
- **Digital processing** for spatial awareness and adaptability
- **Forward-deployed sensing** for predictive noise cancellation

### 1.2 Target Use Case
Industrial workers (e.g., machine operators) who need:
- Protection from harmful machinery noise (target noise X)
- Ability to hear coworkers and safety alerts (ambient sound Y)

### 1.3 Key Innovation
By placing a microphone directly at the noise source, the system knows the exact noise waveform **before** it reaches the user's ear, enabling perfect phase-inverse generation.

---

## 2. System Requirements

### 2.1 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | Cancel target noise X by ≥20dB | Must |
| FR-02 | Preserve ambient sound Y with <3dB attenuation | Must |
| FR-03 | Adapt to user movement in real-time | Must |
| FR-04 | Support frequency range 100Hz - 8kHz | Must |
| FR-05 | Maximum system latency <1ms | Must |
| FR-06 | Wireless range ≥10m between FDM and headset | Should |
| FR-07 | Battery life ≥8 hours continuous use | Should |

### 2.2 Non-Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| NFR-01 | Weight <300g (headset unit) | Must |
| NFR-02 | IP54 dust/water resistance | Should |
| NFR-03 | Comfortable for 8-hour shifts | Must |
| NFR-04 | Setup time <5 minutes | Should |

---

## 3. System Architecture

### 3.1 Signal Flow

```
X(t) ──┬──► [Air Path H₁] ───────────────────────┬──► Ear
       │                                          │
       └──► [FDM] ──► [Wireless] ──► [PCB] ──► [APC H₂] ──► [Speaker H₃] ──┘
                                       ▲
                                       │
                              [UWB/IMU Position Data]
```

### 3.2 Transfer Function Model

The system aims to achieve:
```
Output = X·H₁ + X·H₂·H₃ ≈ 0  (for target noise)
Output = Y·H₁ ≈ Y·H₁         (for ambient sound)
```

Where:
- **H₁(f, d, θ)**: Transfer function from source to ear (depends on frequency, distance, angle)
- **H₂**: Analog phase control (adjusted to create -H₁/H₃)
- **H₃**: Fixed transfer function from speaker to eardrum

### 3.3 H-Map Concept

The **H-Map** is a pre-computed lookup table:
```
H_Map[distance][angle][frequency] = {gain, phase_shift}
```

- Populated during calibration
- Indexed in real-time using UWB/IMU data
- Updated when calibration mic detects suboptimal cancellation

---

## 4. Subsystem Specifications

### 4.1 Forward Deployed Microphone (FDM)

| Parameter | Specification |
|-----------|---------------|
| Type | MEMS Electret |
| Frequency Response | 20Hz - 20kHz |
| SNR | ≥65dB |
| Sensitivity | -26 dBFS |
| Interface | I2S Digital Output |
| Wireless | ESP32 + Custom Protocol |

### 4.2 Spatial Tracking (UWB/IMU)

| Parameter | Specification |
|-----------|---------------|
| UWB Module | DWM3000 / DW3110 |
| Ranging Accuracy | ±10cm |
| Update Rate | 100Hz |
| IMU | BNO055 (9-DOF) |
| Orientation Accuracy | ±2° |
| Sensor Fusion | Kalman Filter |

### 4.3 Main Processing Unit (PCB)

| Parameter | Specification |
|-----------|---------------|
| MCU | STM32F446 (180MHz, FPU) |
| RAM | 128KB SRAM |
| Flash | 512KB |
| ADC | 12-bit, 2.4 MSPS |
| DAC | 12-bit, dual channel |
| Wireless | ESP32-S3 (co-processor) |

### 4.4 Analog Phase Control (APC)

| Parameter | Specification |
|-----------|---------------|
| Topology | All-pass filter network |
| Phase Range | 0° - 360° continuous |
| Frequency Range | 100Hz - 8kHz |
| THD | <0.1% |
| Control | Digital potentiometers |
| Latency | <100μs |

### 4.5 Audio Output

| Parameter | Specification |
|-----------|---------------|
| Driver | Balanced armature + dynamic |
| Frequency Response | 20Hz - 20kHz |
| Impedance | 32Ω |
| Sensitivity | 105dB/mW |

---

## 5. Communication Protocol

### 5.1 FDM to Headset Link

```
┌─────────────────────────────────────────────────┐
│ Packet Structure (Wireless Audio + Sync)        │
├─────────────────────────────────────────────────┤
│ Preamble │ Sync │ Timestamp │ Audio Data │ CRC │
│  2 bytes │ 1B   │   4 bytes │  64 bytes  │ 2B  │
└─────────────────────────────────────────────────┘
```

- **Protocol**: Custom over ESP-NOW (low latency)
- **Sample Rate**: 48kHz
- **Bit Depth**: 16-bit
- **Latency Target**: <5ms wireless

### 5.2 UWB Ranging Protocol

- TWR (Two-Way Ranging) for distance
- TDOA for angle estimation (with multiple anchors)
- Update rate: 100Hz

---

## 6. Calibration Procedure

### 6.1 Initial Setup
1. Place FDM at noise source
2. User wears headset
3. Start calibration mode

### 6.2 H-Map Generation
1. Play swept sine wave from noise source
2. Measure at FDM and calibration mic simultaneously
3. Calculate H₁ for each frequency
4. Repeat at multiple distances/angles
5. Store in H-Map lookup table

### 6.3 Runtime Calibration
- Calibration mic continuously monitors
- If cancellation drops below threshold:
  - Trigger H-Map update for current position
  - Interpolate for nearby positions

---

## 7. Power Budget

| Component | Current (mA) | Voltage | Power (mW) |
|-----------|--------------|---------|------------|
| STM32F446 | 50 | 3.3V | 165 |
| ESP32-S3 | 80 | 3.3V | 264 |
| DWM3000 | 60 | 3.3V | 198 |
| Audio Amp | 30 | 3.3V | 99 |
| Analog Circuits | 20 | ±5V | 200 |
| **Total** | - | - | **926 mW** |

With 2000mAh battery @ 3.7V = 7.4Wh → **~8 hours runtime**

---

## 8. Testing Requirements

### 8.1 Unit Tests
- [ ] FDM frequency response verification
- [ ] UWB ranging accuracy test
- [ ] APC phase shift linearity
- [ ] H-Map lookup speed

### 8.2 Integration Tests
- [ ] End-to-end latency measurement
- [ ] Noise cancellation effectiveness (dB reduction)
- [ ] Ambient sound preservation
- [ ] Multi-position tracking accuracy

### 8.3 System Tests
- [ ] 8-hour battery life test
- [ ] Environmental testing (temperature, humidity)
- [ ] User comfort evaluation
- [ ] Safety compliance

---

## 9. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Wireless latency too high | Medium | High | Fallback to wired FDM option |
| H-Map lookup too slow | Low | High | Pre-compute interpolated values |
| Analog circuit instability | Medium | Medium | Add feedback stabilization |
| UWB multipath errors | Medium | Medium | IMU fusion for smoothing |

---

## 10. References

1. Kuo, S.M., & Morgan, D.R. (1996). Active Noise Control Systems
2. DWM3000 Datasheet, Qorvo
3. STM32F446 Reference Manual, STMicroelectronics

---

*Document Control: This is a living document. Update version number with significant changes.*
