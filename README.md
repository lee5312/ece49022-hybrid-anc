# Spatially Aware Hybrid Active Noise Cancelling (SAHANC)

> ECE 49022 Senior Design Project - Spring 2026

## 🎯 Project Overview

A hybrid noise cancellation system that combines **analog speed** with **digital adaptability** for industrial environments.

### The Problem
- Digital ANC (like AirPods): Too slow for high-frequency noise
- Traditional Analog ANC: Not adaptive (cancels only fixed, known noise)

### Our Solution
**Forward-deployed microphone** at the noise source + **UWB/IMU spatial tracking** + **Analog Phase Control** = Real-time adaptive noise cancellation for industrial workers.

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SYSTEM OVERVIEW                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   [Noise Source X]                              [User's Ear]            │
│         │                                             ▲                  │
│         ▼                                             │                  │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────────┐          │
│   │   FDM Mic   │────▶│   PCB/DSP   │────▶│ Analog Phase    │          │
│   │ (at source) │     │  + H-Map    │     │ Control (APC)   │          │
│   └─────────────┘     └─────────────┘     └─────────────────┘          │
│                              ▲                      │                    │
│                              │                      ▼                    │
│                       ┌─────────────┐     ┌─────────────────┐          │
│                       │  UWB + IMU  │     │ Calibration Mic │          │
│                       │  (spatial)  │     │  (feedback)     │          │
│                       └─────────────┘     └─────────────────┘          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Function | Transfer Function |
|-----------|----------|-------------------|
| FDM (Forward Deployed Mic) | Captures noise X at source | - |
| H₁ | Noise path through space | Pre-calculated based on UWB/IMU |
| H₂ | Analog Phase Control circuit | Real-time adjustment |
| H₃ | External mic to eardrum | Pre-measured, fixed |
| Calibration Mic | Feedback for H-Map updates | Trigger for recalibration |

## 📁 Repository Structure

```
ece49022-hybrid-anc/
├── docs/                    # Documentation
│   ├── specs/              # Technical specifications
│   ├── diagrams/           # Block diagrams, schematics
│   └── reports/            # Weekly reports, presentations
├── firmware/               # Embedded code
│   ├── esp32/             # ESP32 UWB/IMU processing
│   ├── stm32/             # STM32 DSP/control
│   └── common/            # Shared libraries
├── hardware/               # PCB designs
│   ├── kicad/             # KiCad project files
│   ├── gerbers/           # Manufacturing files
│   └── bom/               # Bill of materials
├── software/               # PC/Mobile software
│   ├── calibration/       # Calibration tools
│   └── visualization/     # Data visualization
├── simulations/            # MATLAB/Simulink models
│   ├── transfer_func/     # H₁, H₂, H₃ models
│   └── noise_analysis/    # Noise characterization
└── tests/                  # Test procedures & results
```

## 🛠️ Tech Stack

### Hardware
- **MCU**: STM32F4 (DSP) + ESP32 (Wireless/UWB)
- **UWB**: DWM3000 module
- **IMU**: MPU-6050 / BNO055
- **Audio**: High-quality MEMS microphones
- **Analog**: Op-amp based phase control circuit

### Firmware
- **Language**: C/C++
- **RTOS**: FreeRTOS
- **Build**: PlatformIO / STM32CubeIDE

### Software
- **Calibration**: Python
- **Simulation**: MATLAB/Simulink

## 👥 Team

| Name | Role | Subsystem |
|------|------|-----------|
| TBD | Team Lead | Integration |
| TBD | Hardware | PCB Design |
| TBD | Firmware | Embedded Systems |
| TBD | DSP | Signal Processing |
| TBD | Sensors | UWB/IMU Integration |

## 📅 Timeline

- **Week 1-4**: Conceptual Design & Component Selection
- **Week 5-8**: Schematic Design & Simulation
- **Week 9-12**: PCB Fabrication & Firmware Development
- **Week 13-16**: Integration & Testing
- **Week 17**: Final Demonstration

## 🚀 Getting Started

```bash
# Clone the repository
git clone https://github.com/lee5312/ece49022-hybrid-anc.git

# Install PlatformIO (for firmware development)
pip install platformio

# Build firmware
cd firmware/esp32
pio build
```

## 📄 Documentation

- [Technical Specification](docs/specs/TECHNICAL_SPEC.md)
- [Hardware Design Guide](docs/specs/HARDWARE_SPEC.md)
- [Firmware Architecture](docs/specs/FIRMWARE_SPEC.md)
- [Calibration Procedure](docs/specs/CALIBRATION.md)

## 📜 License

This project is for educational purposes as part of Purdue ECE 49022.

---

*Last updated: February 2026*
