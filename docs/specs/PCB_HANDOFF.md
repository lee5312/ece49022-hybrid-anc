# ECE 49022 — Dae: PCB Handoff Document
## Sensor Subsystem → PCB Layout

> Date: 2026-03-08
> From: Sensor/Firmware → PCB
> MCU Board: **PJRC Teensy 4.1** (NXP iMXRT1062, Cortex-M7, 600 MHz) — without Ethernet

---

## 1. MCU Connections (Pin Map)

### 1.0 MCU — Teensy 4.1

| Parameter | Value |
|-----------|-------|
| MCU | NXP iMXRT1062, ARM Cortex-M7, 600 MHz |
| Flash | 8 MB (internal QSPI) |
| RAM | 1024 KB (512 KB tightly-coupled + 512 KB) |
| I/O Voltage | 3.3V (5V tolerant on digital pins) |
| Board Size | 61 × 18 mm |
| USB | USB-C (native USB serial for debug telemetry) |
| SPI Buses | SPI (default), SPI1, SPI2 (bottom pads) |
| Upload | USB (Teensyduino / PlatformIO) |

### 1.1 GPIO Usage Summary

| Teensy Pin | Function | Module | Bus | Direction |
|------------|----------|--------|-----|-----------|
| 0 | CS (Top) | DWM3000 #1 | — | Output |
| 1 | **SPI1 MISO** | DWM3000 ×4 (shared) | SPI1 | Input |
| 2 | RST (Top) | DWM3000 #1 | — | Output |
| 3 | IRQ (Top) | DWM3000 #1 | — | Input |
| 4 | CS (Left) | DWM3000 #2 | — | Output |
| 5 | RST (Left) | DWM3000 #2 | — | Output |
| 6 | IRQ (Left) | DWM3000 #2 | — | Input |
| 7 | CS (Right) | DWM3000 #3 | — | Output |
| 8 | RST (Right) | DWM3000 #3 | — | Output |
| 9 | IRQ (Right) | DWM3000 #3 | — | Input |
| 10 | **SPI CS** | LSM6DS3TR-C | SPI | Output |
| 11 | **SPI MOSI** | LSM6DS3TR-C | SPI | Output |
| 12 | **SPI MISO** | LSM6DS3TR-C | SPI | Input |
| 13 | **SPI SCK** | LSM6DS3TR-C | SPI | Output |
| 24 | CS (Tool) | DWM3000 #4 | — | Output |
| 25 | RST (Tool) | DWM3000 #4 | — | Output |
| 26 | **SPI1 MOSI** | DWM3000 ×4 (shared) | SPI1 | Output |
| 27 | **SPI1 SCK** | DWM3000 ×4 (shared) | SPI1 | Output |
| 28 | IRQ (Tool) | DWM3000 #4 | — | Input |
| **14 (A0)** | **Analog In** | SPW2430 MIC_TOOL (ADC direct) | ADC | Input |
| **15 (A1)** | **Analog In** | SPW2430 MIC_OUT_L (ADC direct) | ADC | Input |
| **16 (A2)** | **Analog In** | SPW2430 MIC_OUT_R (ADC direct) | ADC | Input |
| **17 (A3)** | **Analog In** | SPW2430 MIC_IN_L (via pre-amp) | ADC | Input |
| **18 (A4)** | **Analog In** | SPW2430 MIC_IN_R (via pre-amp) | ADC | Input |
| USB | Serial (debug telemetry) | Host PC | — | Bidirectional |

**Total: 24 pins used** (4 SPI bus + 3 SPI1 bus + 12 GPIO for CS/RST/IRQ + 5 ADC)
**Free pins:** 19–23, 29–41 (plus bottom pads 42–54)

### 1.2 SPI Bus Configuration

| Parameter | SPI (IMU) | SPI1 (UWB ×4) |
|-----------|-----------|---------------|
| Mode | Mode 3 (CPOL=1, CPHA=1) | Mode 0 (CPOL=0, CPHA=0) |
| Init Clock | 4 MHz | 2 MHz |
| Operational Clock | 4 MHz | **2 MHz** (see note below) |
| Max Clock | 10 MHz | 38 MHz |
| SCK | Pin 13 | Pin 27 |
| MOSI | Pin 11 | Pin 26 |
| MISO | Pin 12 | Pin 1 |
| CS | Pin 10 (software) | Software CS per module |

> **⚠ SPI1 clock fixed at 2 MHz (Tool cable distance):** The Tool DWM3000 is mounted on a tool located **60 cm – 1 m** away from the headset. Standard SPI assumes distances within about 15 cm, so signal integrity (SI) is not guaranteed at 16 MHz over this length. **Without any circuit change**, fix the SPI1 clock to **2 MHz** in firmware. The data volume required for 5 Hz updates from four UWB modules is easily handled at 2 MHz (four-module ranging completes within 200 ms). Physical mitigation: use a **shielded cable** (see §3.4 #5).

### 1.3 Power Rails

| Rail | Source | Consumers |
|------|--------|-----------|
| 3.3V_EXT | **External LDO (required)** — AP2112K-3.3 (600 mA) | DWM3000 ×4, SPW2430 ×5, MCP6002 ×1 (pre-amp) |
| 3.3V_MCU | Teensy 4.1 onboard regulator (250 mA) | LSM6DS3TR-C (IMU), Teensy internal only |
| GND | Common | All modules |

> **⚠ LDO is required:** Current estimate for 4 DWM3000 modules: 3× RX idle (~10 mA each) + 1× TX (~120 mA) = **~150 mA** (TDMA). SPW2430 ×5 = ~2.5 mA. Total is ~153 mA, which leaves insufficient margin on the Teensy onboard regulator (250 mA), especially after accounting for the Teensy board's own consumption. **An external 3.3V LDO must be populated as a separate rail (3.3V_EXT).**

---

## 2. Schematics

### 2.1 IMU — LSM6DS3TR-C (SPI)

```
                    Teensy 4.1                     LSM6DS3TR-C (LGA-14L)
                   ┌─────────────┐                ┌──────────────┐
                   │             │                │              │
        Pin 13 ────│ SPI SCK     │────────────────│ SCL (Pin 13) │
        Pin 11 ────│ SPI MOSI    │────────────────│ SDA (Pin 14) │
        Pin 12 ────│ SPI MISO    │────────────────│ SDO (Pin 1)  │
        Pin 10 ────│ SPI CS      │────────────────│ CS  (Pin 12) │
                   │             │                │              │
                   │        3.3V │──┬─────────────│ VDD (Pin 8)  │
                   │             │  └─────────────│ VDDIO(Pin 5) │
                   │         GND │──┬─────────────│ GND (Pin 6)  │
                   │             │  ├─────────────│ GND (Pin 7)  │
                   │             │  │             │              │
                   └─────────────┘  │             │ SDx (Pin 2)──┤── GND (Mode 1)
                                    │             │ SCx (Pin 3)──┤── GND (Mode 1)
                                    │             │ INT1 (4) ────┤── N/C
                                    │             │ INT2 (9) ────┤── N/C
                                    │             │ NC (Pin 10)──┤── N/C
                                    │             │ NC (Pin 11)──┤── N/C
                                    │             └──────────────┘
                                    │
                             100nF ═══ decoupling cap (one each for VDD and VDDIO)
                                    │
                                   GND
```

**LSM6DS3TR-C pin connection summary (LGA-14L):**

| Pin # | Name | Connect To | Notes |
|-------|------|-----------|-------|
| 1 | SDO/SA0 | Teensy Pin 12 (MISO) | SPI data out / I2C address LSB |
| 2 | SDx | GND | Mode 1 (aux I2C unused), tie to GND or VDDIO |
| 3 | SCx | GND | Mode 1 (aux I2C unused), tie to GND or VDDIO |
| 4 | INT1 | N/C | Interrupt unused |
| 5 | VDDIO | 3.3V | I/O power, 100nF decap |
| 6 | GND | GND | |
| 7 | GND | GND | |
| 8 | VDD | 3.3V | Core power, 100nF decap |
| 9 | INT2 | N/C | Interrupt unused |
| 10 | NC | N/C | Leave unconnected, solder pad only |
| 11 | NC | N/C | Leave unconnected, solder pad only |
| 12 | CS | Teensy Pin 10 | Active low (0 = SPI mode) |
| 13 | SCL | Teensy Pin 13 (SCK) | SPI serial port clock (SPC) |
| 14 | SDA | Teensy Pin 11 (MOSI) | SPI serial data input (SDI) |

### 2.2 UWB — DWM3000 (SPI1 shared bus, ×4 modules)

```
Teensy 4.1                     DWM3000 module

Pin 27 (SPI1 SCK)  ──────────> SPICLK   (Pin 20)
Pin 26 (SPI1 MOSI) ──────────> SPIMOSI  (Pin 18)
Pin 1  (SPI1 MISO) <────────── SPIMISO  (Pin 19)

CS GPIO            ──────────> SPICSn   (Pin 17)
RST GPIO           ──────────> RSTn     (Pin 3)
IRQ GPIO           <────────── IRQ/GPIO8 (Pin 22)

3.3V_EXT           ──────────> VDD3V3   (Pins 10,24)
GND                ──────────> VSS      (Pins 9,11,23,25)

WAKEUP (Pin 2)     ──────────> GND (tie low)
GPIO5  (Pin 15)    ──────────> N/C
GPIO6  (Pin 16)    ──────────> N/C
EXTON  (Pin 21)    ──────────> N/C

Local decoupling near module:
3.3V_EXT ──║║── GND    (100nF, each module)

IRQ/GPIO8 line:
Pin 22 ──[100kΩ]── GND
```

**Per-Module CS/RST/IRQ Assignment:**

| Module | Role | CS Pin | RST Pin | IRQ Pin |
|--------|------|--------|---------|---------|
| **T** (Top) | Headset Initiator | Pin 0 | Pin 2 | Pin 3 |
| **L** (Left) | Headset Initiator | Pin 4 | Pin 5 | Pin 6 |
| **R** (Right) | Headset Initiator | Pin 7 | Pin 8 | Pin 9 |
| **Tool** (Fixed) | Responder | Pin 24 | Pin 25 | Pin 28 |

**DWM3000 pin connection summary (24-pin side-castellated, 23×13 mm):**

| Pin # | Name | Connect To | Notes |
|-------|------|-----------|-------|
| 1 | GPIO1 | N/C | |
| 2 | WAKEUP | GND | Sleep unused, tie low |
| 3 | RSTn | Teensy (per module) | Active low. External 10kΩ pull-up to 3.3V |
| 4–8 | — | — | — |
| 9 | VSS | GND | **Must connect** |
| 10 | VDD3V3 | 3.3V | Module internal 3.3V→1.8V regulator, 100nF decap |
| 11 | VSS | GND | **Must connect** |
| 12–16 | GPIO2–6 | N/C | Unused |
| 17 | SPICSn | Teensy (per module) | Active low |
| 18 | SPIMOSI | Pin 26 (bus) | |
| 19 | SPIMISO | Pin 1 (bus) | |
| 20 | SPICLK | Pin 27 (bus) | |
| 21 | EXTON | N/C | |
| 22 | IRQ/GPIO8 | Teensy (per module) | **100kΩ pull-down required** |
| 23 | VSS | GND | **Must connect** |
| 24 | VDD3V3 | 3.3V | Same as Pin 10 |
| 25 | VSS | GND | **Must connect** |

### 2.3 MEMS Microphones — SPW2430 ×5 (Analog)

Five Adafruit SPW2430 MEMS Microphone Breakouts are used. The **three external microphones** are wired directly to the ADC (no pre-amp), and the **two internal calibration microphones** pass through the pre-amp (§2.4, Gain = 21) before connecting to the Teensy ADC.

**Microphone assignment table:**

| Mic ID | Role | Location | Path | ADC Pin | APC |
|--------|------|----------|------|---------|-----|
| MIC_TOOL | External | Target tool (wired) | **ADC direct** (LPF only) | Pin 14 (A0) | — |
| MIC_OUT_L | External | Left outside of headset | **ADC direct** (LPF only) | Pin 15 (A1) | — |
| MIC_OUT_R | External | Right outside of headset | **ADC direct** (LPF only) | Pin 16 (A2) | — |
| MIC_IN_L | Calibration | Left inside of headset | **Pre-amp §2.4** (Gain = 21) | Pin 17 (A3) | **APC input** |
| MIC_IN_R | Calibration | Right inside of headset | **Pre-amp §2.4** (Gain = 21) | Pin 18 (A4) | **APC input** |

**SPW2430 breakout connections (same for all 5):**

| Breakout Pad | Connect To | Notes |
|-------------|-----------|-------|
| VCC | 3.3V | 1.5V–3.6V, use 3.3V |
| GND | GND | |
| OUT | → external: R_LP → ADC node, C_LP is ADC node→GND | or → internal: Pre-amp → R_LP → ADC node, C_LP is ADC node→GND |

**Signal paths:**

```
External microphones (MIC_TOOL, MIC_OUT_L, MIC_OUT_R) — no pre-amp, up to ~110 dB SPL:
  SPW2430.OUT → [R_LP 1.5kΩ] ─┬→ Teensy ADC
                               └→ [C_LP 10nF] → GND

Calibration microphones (MIC_IN_L, MIC_IN_R) — pre-amp Gain = 21, 60–85 dB inside earmuff:
  SPW2430.OUT → [Pre-amp §2.4] ─┬→ APC circuit input (Ahmed subsystem)
                                 └→ [R_LP 1.5kΩ] ─┬→ Teensy ADC
                                                    └→ [C_LP 10nF] → GND
```

> **Design rationale:** External microphones must capture industrial noise up to ~110 dB SPL without clipping. Removing the Gain = 21 pre-amp raises the ADC clipping threshold substantially from ~90 dB to **~112 dB**. The internal calibration microphones receive only 60–85 dB in practice because the earmuff provides ~20–30 dB of passive attenuation, so keeping Gain = 21 maximizes ADC resolution.

> **MIC_TOOL wiring:** Wired together with the Tool DWM3000 (distance ~60 cm–1 m). Wire bundle = SPI1 bus 3 lines + CS + RST + IRQ + **Mic OUT** + VCC + GND = **9 wires total**. Use **CAT6 shielded Ethernet cable** or a shielded multi-core cable (SPI SCK ↔ GND twisted pair recommended). Fixing SPI1 to 2 MHz secures SI margin (see §1.2).

> **Calibration mic dual routing:** The pre-amp output of MIC_IN_L/R must branch to the APC circuit **before** the anti-alias LPF.

### 2.4 Pre-Amplifier Circuit — MCP6002 (×1, internal microphones only)

The SPW2430 analog output is only on the order of millivolts, so a pre-amp is required to use the Teensy 12-bit ADC (0–3.3V) effectively.

**Design specifications:**

| Parameter | Value | Notes |
|-----------|-------|-------|
| Topology | Non-inverting amplifier | High-impedance input |
| Gain | 21× (26.4 dB) | 1 + R_F/R_G = 1 + 20kΩ/1kΩ |
| Input coupling | AC (1 µF) | Removes DC bias |
| Bias point | VCC/2 = 1.65V | 100kΩ/100kΩ divider |
| Anti-alias LPF | 10.6 kHz, 1st-order RC | 1.5kΩ + 10nF |
| Op-amp | MCP6002 (Microchip) | Dual, RRIO, 1 MHz GBW, SOIC-8 |
| Supply | 3.3V single-rail | |
| Channels | 2 (1× MCP6002 dual, internal microphones only) | |

**Single-channel schematic (repeat ×2 for internal microphones):**

```
  VBIAS generation:

    3.3V
     │
   [R_B1] 100kΩ
     │
     ├────────── VBIAS (≈ 1.65V)
     │
   [R_B2] 100kΩ
     │
    GND


  Signal path (1 channel):

    SPW2430.OUT
        │
      [C_IN] 1uF
        │
        o───────────────┬───────────────┐
        │               │               │
        │               └──── VBIAS ────┤
        │                               │
        │        ┌──────────────────────▼──────┐
        └────────┤ +                       U1A │
                 │        MCP6002              ├────o────> APC input
        o────────┤ -                           │    │
        │        └─────────────────────────────┘    │
        │                                           │
      [R_F] 20kΩ                                    │
        │                                           │
        └───────────────────────────────────────────┘
        │
      [R_G] 1kΩ
        │
      [C_G] 10uF
        │
       GND


    ADC anti-alias LPF:

    MCP6002.OUT ----[R_LP 1.5kΩ]----o----> Teensy ADC
                          |
                        [C_LP] 10nF
                          |
                         GND
```

> **Simplification note:** MCP6002 power pins are omitted in the ASCII diagram, but the actual wiring is VDD = Pin 8 → 3.3V_EXT, VSS = Pin 4 → GND, with one 100nF decoupling capacitor placed near VDD.

> **How to read this:** The microphone output passes through `C_IN` and is AC-coupled around `VBIAS` into the MCP6002 `+` input. The `-` input uses the `R_F/R_G/C_G` network to set Gain = 21, and `C_G` blocks DC to prevent the output from sticking against the 3.3V rail. The op-amp output first branches to the APC path, then passes through the `R_LP/C_LP` low-pass filter into the Teensy ADC.

**Gain = 1 + R_F / R_G = 1 + 20kΩ / 1kΩ = 21 (26.4 dB)**

**Operating range:**

| SPL (dB) | Environment | SPW2430 Vrms | Amplified Vrms | ADC Range (V) | Status |
|----------|-------------|--------------|----------------|---------------|--------|
| 60 | Quiet room | 2.5 mV | 53 mV | 1.58–1.73 | ✓ OK |
| 70 | Conversation | 8.0 mV | 168 mV | 1.41–1.89 | ✓ OK |
| 85 | Factory noise | 44.8 mV | 941 mV | 0.32–2.98 | ✓ OK |
| 90 | Loud noise | 79.6 mV | 1.67 V | clips | ⚠ soft clip |
| 94 | Reference 1 Pa | 126 mV | 2.65 V | clips | ⚠ |

> **Design target:** Use about 80% of the ADC range at 85 dB SPL. Above 90 dB, the op-amp output softly clips at the rail, but there is no distortion in the normal ANC operating range (60–85 dB).
>
> **120 dB SPL note:** DD §4.3.2 specifies a maximum 120 dB SPL environment, but at the SPW2430 AOP (~120 dB, THD 10%), the output reaches ~2.5 Vrms, which clips on a 3.3V supply. However, due to ~20–30 dB passive attenuation from the earmuff, the **internal calibration microphones (MIC_IN_L/R) only receive about 90–100 dB in practice**, so ANC feedback-loop operation is unaffected. The external microphones (MIC_OUT_L/R) are allowed to clip in a 120 dB environment because they serve as noise references.

**MCP6002 channel assignment (SOIC-8) — internal microphones only, ×1:**

| IC | Ch A (Pin 1 out) | Ch B (Pin 7 out) |
|----|------------------|------------------|
| U1 | MIC_IN_L → Pin 17 (A3) | MIC_IN_R → Pin 18 (A4) |

> The three external microphones (MIC_TOOL, MIC_OUT_L, MIC_OUT_R) connect directly to the ADC without a pre-amp; only the LPF is applied.

**Pre-amp parts list (internal microphone 2-channel only):**

| Component | Value | Package | Qty | Notes |
|-----------|-------|---------|-----|-------|
| MCP6002-I/SN | Dual Op-Amp | SOIC-8 | 1 | Rail-to-rail I/O, 1.8–6V |
| C_IN | 1 µF | 0402 MLCC | 2 | AC coupling, X7R |
| R_B1, R_B2 | 100 kΩ | 0402 | 4 | Bias divider (2 per channel) |
| R_F | 20 kΩ | 0402 | 2 | Feedback resistor |
| R_G | 1 kΩ | 0402 | 2 | Ground-reference resistor |
| C_G | 10 µF | 0603 MLCC | 2 | DC blocking to GND (1 per channel) |
| R_LP | 1.5 kΩ | 0402 | 5 | Anti-alias LPF (all 5 channels) |
| C_LP | 10 nF | 0402 MLCC | 5 | Anti-alias, fc ≈ 10.6 kHz (all 5 channels) |
| C_BYP | 100 nF | 0402 MLCC | 1 | Op-amp VDD decoupling |

### 2.5 PCB Design Notes

1. **Decoupling:** Place a 100nF MLCC close to each VDD pin (IMU: VDD + VDDIO each, DWM3000: each module VDD3V3).
2. **SPI1 bus:** Four DWM3000 modules share MISO/MOSI/SCK. Keep traces short and matched in length.
3. **IRQ pull-down:** Each of the four DWM3000 IRQ pins requires a 100kΩ pull-down (per DWM3000 datasheet requirement).
4. **RSTn pull-up:** Each DWM3000 RSTn line requires a 10kΩ pull-up to 3.3V (open-drain output).
5. **Ground vias:** Provide sufficient ground vias under each DWM3000 module (affects RF performance).
6. **UWB antenna clearance:** Remove copper under the DWM3000 antenna area (keepout zone). See module datasheet §5.
7. **Teensy socketing:** Mount the Teensy 4.1 on the main PCB via pin headers. Dual-row spacing = 0.6" (15.24 mm), pin pitch = 0.1" (2.54 mm).
8. **Power (required):** Because of the peak current of four DWM3000 modules, an external 3.3V LDO **must be populated** (see §1.3). Use the Teensy 3.3V pin for the IMU only; power the remaining sensors from the separate LDO rail.
9. **Microphone routing:** Keep SPW2430 analog signal lines as far as possible from SPI/digital traces. Place the anti-alias RC filter (R_LP + C_LP) near the Teensy ADC pins.
10. **Tool cable connector:** Place a robust 9-pin or larger connector on the PCB edge so the 9-wire cable to the tool can be secured reliably (RJ45 jack, JST, or sturdy right-angle header, etc.).

---

## 3. Sensor Placement (Physical Positions)

### 3.1 Coordinate System

```
        +Z (up)
         │
         │
         │    +Y (forward, user line-of-sight direction)
         │   ╱
         │  ╱
         │ ╱
         └──────── +X (right)

     Origin = center of headband (top center of head)
```

### 3.2 Headset Sensor Layout (Top View)

```
                    ┌─── Front (face direction) ────┐
                    │                               │
                    │         [T] Top               │
                    │        (0, 0, +10cm)          │
                    │            │                  │
                    │            │ 10cm             │
                    │            │                  │
          ──────────[L]─────────── ● ─────────────[R]──────────
          Left     (-8cm,0,0)    Origin     (+8cm,0,0)    Right
                    │         (0,0,0)               │
                    │                               │
                    │    [IMU] + [Teensy 4.1]       │
                    │     (at Origin,               │
                    │      headband center)         │
                    │                               │
                    └─── Back (rear of head) ───────┘
```

### 3.3 Detailed Sensor Placement

| Sensor | Position (body frame) | Physical placement description | Mounting method |
|--------|------------------------|-------------------------------|-----------------|
| **IMU** (LSM6DS3TR-C) | (0, 0, 0) — Origin | Headband center, soldered directly on main PCB | PCB onboard |
| **Teensy 4.1** | (0, 0, 0) — Origin | Headband center, mounted on main PCB with headers | Socket / pin headers |
| **UWB T** (DWM3000 #1) | (0, 0, +10cm) | Top of head, upper headband | FPC or wire connection |
| **UWB L** (DWM3000 #2) | (−8cm, 0, 0) | Above left ear, left side of headband | FPC or wire connection |
| **UWB R** (DWM3000 #3) | (+8cm, 0, 0) | Above right ear, right side of headband | FPC or wire connection |
| **UWB Tool** (DWM3000 #4) | World: (0, 1.5m, 0.8m) | Fixed to the tool | Wired connection (SPI1 + GPIO) |
| **MIC_TOOL** (SPW2430) | World: same position as Tool | Mounted on the tool together with the Tool DWM3000 | Wire → LPF → Pin 14 (ADC direct) |
| **MIC_OUT_L** (SPW2430) | Left outside of headset | Outside left earmuff | Wire → LPF → Pin 15 (ADC direct) |
| **MIC_OUT_R** (SPW2430) | Right outside of headset | Outside right earmuff | Wire → LPF → Pin 16 (ADC direct) |
| **MIC_IN_L** (SPW2430) | Left inside of headset | Inside left earmuff, near eardrum | Wire → pre-amp → LPF → Pin 17 + APC |
| **MIC_IN_R** (SPW2430) | Right inside of headset | Inside right earmuff, near eardrum | Wire → pre-amp → LPF → Pin 18 + APC |

### 3.4 Placement Constraints

1. **UWB T/L/R must face different directions:** Place the three DWM3000 antennas in near-orthogonal directions to minimize DOP.
2. **IMU near the rotation center:** Place it at the center of the headband to minimize lever-arm effects.
3. **Maintain UWB antenna LOS:** Place DWM3000 antennas outward so they are not blocked by the head or helmet.
4. **Tool + MIC_TOOL share a fixed position:** The Tool DWM3000 and MIC_TOOL SPW2430 are mounted together on the tool. They connect by wire to the headset mainboard (Teensy).
5. **Tool wire bundle (~1 m):** 9-wire cable between headset and tool (SPI1 bus 3 lines + CS + RST + IRQ + Mic OUT + VCC + GND). Use **CAT6 shielded (STP) Ethernet cable** or a shielded multi-core cable ← 4 twisted pairs = 8 wires + drain. Recommended pairing: SCK↔GND, MOSI↔MISO, CS↔IRQ, Mic OUT↔VCC. SPI1 clock is **fixed at 2 MHz** (§1.2); at this speed, a 1 m shielded cable has sufficient SI margin.
6. **Anti-alias RC placement:** Place R_LP + C_LP near the Teensy ADC pins. Keep microphone analog routing as far as possible from SPI/digital traces.

> **Caution — Tool wire length:** At SPI1 16 MHz, wire length is typically recommended to stay within ~15 cm. If a longer distance is required, lower the SPI clock to 2 MHz or consider a separate MCU.

---

## 4. DigiKey Part Numbers

| Component | Manufacturer Part Number | DigiKey Part Number | Qty | Description |
|-----------|--------------------------|---------------------|-----|-------------|
| **MCU Board** | Teensy 4.1 (w/o Ethernet) | 1568-DEV-20360-ND* | 1 | PJRC Teensy 4.1, iMXRT1062, 600 MHz, 61×18 mm |
| **UWB Module** | DWM3000 | DWM3000-ND | 4 | Qorvo UWB transceiver module, 23×13 mm, side-castellated |
| **IMU** | LSM6DS3TR-C | 497-17232-1-ND | 1 | STMicro 6-axis IMU, LGA-14L, 2.5×3×0.83 mm |
| **Decoupling Cap** | 100nF 0402 MLCC | — (generic) | 6+ | 100nF, 16V, X7R, one for each VDD |
| **IRQ Pull-down** | 100kΩ 0402 | — (generic) | 4 | For DWM3000 IRQ pins |
| **RST Pull-up** | 10kΩ 0402 | — (generic) | 4 | For DWM3000 RSTn pins |
| **MEMS Microphone** | Adafruit SPW2430 | 1528-1396-ND | **5** | MEMS mic breakout, Knowles SPW2430HR5H-B |
| **Pre-amp Op-Amp** | MCP6002-I/SN | MCP6002-I/SN-ND | **1** | Dual RRIO op-amp, SOIC-8 (dedicated to 2 internal mic channels) |
| **Coupling Cap (C_IN)** | 1 µF 0402 X7R | — (generic) | 2 | Pre-amp AC coupling (internal microphones only) |
| **Bias Resistor (R_B)** | 100 kΩ 0402 | — (generic) | 4 | Pre-amp VBIAS divider |
| **Gain Resistor (R_F)** | 20 kΩ 0402 | — (generic) | 2 | Pre-amp feedback |
| **Gain Resistor (R_G)** | 1 kΩ 0402 | — (generic) | 2 | Pre-amp ground reference |
| **Ground Capacitor (C_G)** | 10 µF 0603 X5R/X7R | — (generic) | 2 | Pre-amp DC blocking to GND |
| **LPF Resistor (R_LP)** | 1.5 kΩ 0402 | — (generic) | 5 | Anti-alias filter (common to all 5 channels) |
| **LPF Cap (C_LP)** | 10 nF 0402 X7R | — (generic) | 5 | Anti-alias, fc ≈ 10.6 kHz |
| **3.3V LDO (required)** | AP2112K-3.3TRG1 | AP2112K-3.3TRG1DICT-ND | 1 | 600mA LDO, SOT-23-5, **must populate** |

> *The Teensy 4.1 can also be purchased directly from PJRC (pjrc.com) or from SparkFun (DEV-20360) in addition to DigiKey. DigiKey part numbers may change depending on stock status, so verify before purchase.*

### 4.1 Part Details

**Teensy 4.1 (PJRC)**
- MPN: Teensy 4.1 (PJRC #4622, without Ethernet)
- MCU: NXP iMXRT1062, ARM Cortex-M7 @ 600 MHz
- Flash: 8 MB, RAM: 1024 KB
- USB: Native USB (Serial, MIDI, etc.)
- Board: 61 × 18 mm, dual-row 0.1" headers

**DWM3000 (Qorvo)**
- MPN: `DWM3000`
- Package: 24-pin side-castellated module, 23 × 13 × 2.9 mm
- Internal IC: DW3110 (DW3000 family)
- Supply: 2.5V–3.6V (VDD3V3), internal 1.8V regulator included
- UWB Channels: 5 (6.5 GHz), 9 (8 GHz)
- Includes onboard antenna

**Adafruit SPW2430 MEMS Microphone Breakout**
- Adafruit Product #: 2716
- MEMS Mic IC: Knowles SPW2430HR5H-B
- Sensitivity: −18 dBV/Pa @ 94 dB SPL (126 mVrms/Pa)
- SNR: 63 dB (A-weighted)
- Output: Analog, ~VCC/2 DC bias (~1.65V)
- Supply: 1.5V–3.6V (use 3.3V)
- Breakout size: 14 × 10 mm
- Quantity: **5 units** (MIC_TOOL, MIC_OUT_L/R, MIC_IN_L/R)
- External 3 (MIC_TOOL/OUT_L/OUT_R): **ADC direct** (anti-alias RC LPF only, up to ~110 dB SPL)
- Internal 2 (MIC_IN_L/IN_R): **Pre-amp Gain = 21** (60–85 dB inside earmuff)

**MCP6002-I/SN (Microchip) — Pre-amp Op-Amp**
- Package: SOIC-8 (dual op-amp)
- Supply: 1.8V–6.0V (3.3V single-rail used)
- Rail-to-rail input & output
- GBW: 1 MHz (at gain = 21, BW ≈ 48 kHz)
- Input noise: 28 nV/√Hz
- Quantity: **1 unit** (dedicated to MIC_IN_L + MIC_IN_R, 2 channels)

**LSM6DS3TR-C (STMicroelectronics)**
- MPN: `LSM6DS3TR-C`
- Package: LGA-14L, 2.5 × 3.0 × 0.83 mm
- Supply: 1.71V–3.6V
- Accel: ±2/±4/±8/±16 g (design uses ±2g)
- Gyro: ±125/±245/±500/±1000/±2000 dps (design uses ±245 dps)
- Interface: SPI / I²C (design uses SPI Mode 3)

---

## Quick Reference: Net List

```
NET: SPI_SCK      Pin 13  ↔  IMU.SCL
NET: SPI_MOSI     Pin 11  ↔  IMU.SDA
NET: SPI_MISO     Pin 12  ↔  IMU.SDO
NET: SPI_CS       Pin 10  ↔  IMU.CS

NET: SPI1_SCK     Pin 27  ↔  UWB_T.SPICLK, UWB_L.SPICLK, UWB_R.SPICLK, UWB_TOOL.SPICLK
NET: SPI1_MOSI    Pin 26  ↔  UWB_T.SPIMOSI, UWB_L.SPIMOSI, UWB_R.SPIMOSI, UWB_TOOL.SPIMOSI
NET: SPI1_MISO    Pin 1   ↔  UWB_T.SPIMISO, UWB_L.SPIMISO, UWB_R.SPIMISO, UWB_TOOL.SPIMISO

NET: UWB_T_CS     Pin 0   ↔  UWB_T.SPICSn
NET: UWB_T_RST    Pin 2   ↔  UWB_T.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_T_IRQ    Pin 3   ↔  UWB_T.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_L_CS     Pin 4   ↔  UWB_L.SPICSn
NET: UWB_L_RST    Pin 5   ↔  UWB_L.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_L_IRQ    Pin 6   ↔  UWB_L.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_R_CS     Pin 7   ↔  UWB_R.SPICSn
NET: UWB_R_RST    Pin 8   ↔  UWB_R.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_R_IRQ    Pin 9   ↔  UWB_R.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_TOOL_CS  Pin 24  ↔  UWB_TOOL.SPICSn
NET: UWB_TOOL_RST Pin 25  ↔  UWB_TOOL.RSTn  (+ 10kΩ pull-up to 3.3V)
NET: UWB_TOOL_IRQ Pin 28  ↔  UWB_TOOL.IRQ   (+ 100kΩ pull-down to GND)

NET: MIC_TOOL     Pin 14 (A0) ↔  SPW2430.OUT → R_LP/C_LP           (direct, shielded wire)
NET: MIC_OUT_L    Pin 15 (A1) ↔  SPW2430.OUT → R_LP/C_LP           (direct)
NET: MIC_OUT_R    Pin 16 (A2) ↔  SPW2430.OUT → R_LP/C_LP           (direct)
NET: MIC_IN_L     Pin 17 (A3) ↔  SPW2430.OUT → U1A (pre-amp) ─┬→ R_LP/C_LP  (+ APC tee)
NET: MIC_IN_R     Pin 18 (A4) ↔  SPW2430.OUT → U1B (pre-amp) ─┬→ R_LP/C_LP  (+ APC tee)

NET: PREAMP_VCC   3.3V_EXT ↔  U1.VDD(8) + 100nF decap
NET: PREAMP_GND   GND      ↔  U1.VSS(4)

NET: 3V3_EXT      External LDO 3.3V out  ↔  DWM3000 ×4 VDD3V3 + SPW2430 ×5 VCC + MCP6002 ×1
NET: 3V3_MCU      Teensy 3.3V             ↔  IMU VDD + IMU VDDIO (Teensy internal only)
NET: LDO_IN       Teensy VIN (USB 5V)     ↔  AP2112K VIN (LDO input)
NET: GND          Common ground ↔  All VSS/GND pins

NET: USB          Teensy USB-C ↔  Host PC (debug serial telemetry)
```