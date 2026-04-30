# ECE 49022 — Dae: PCB Handoff Document (STM32)
## Sensor Subsystem → PCB Layout

> Date: 2026-03-10
> From: Sensor/Firmware → PCB
> MCU: **STM32F446RCT6** (STMicroelectronics, ARM Cortex-M4F, 180 MHz, LQFP-64)

---

## 1. MCU Connections (Pin Map)

### 1.0 MCU — STM32F446RCT6

| Parameter | Value |
|-----------|-------|
| MCU | STM32F446RCT6, ARM Cortex-M4 + DSP + single-precision FPU, 180 MHz |
| Package | LQFP-64, 10 × 10 mm, 0.5 mm pitch |
| Flash | 256 KB |
| RAM | 128 KB SRAM |
| I/O Voltage | 3.3V design (VDD/VDDA 1.7V–3.6V) |
| ADC | 3× 12-bit ADC, up to 2.4 MSPS |
| DAC | 2× 12-bit DAC |
| SPI / I2S | Up to 4× SPI, 3× multiplexed I2S |
| USB | USB 2.0 FS device/OTG (PA11 / PA12) |
| Upload / Debug | SWD (recommended), USB DFU (only if BOOT0 can be driven high) |

### 1.1 GPIO Usage Summary

| MCU Pin | Function | Module | Bus | Direction |
|---------|----------|--------|-----|-----------|
| PA4 | IMU_CS | LSM6DS3TR-C | SPI1 | Output |
| PA5 | **SPI1_SCK** | LSM6DS3TR-C | SPI1 | Output |
| PA6 | **SPI1_MISO** | LSM6DS3TR-C | SPI1 | Input |
| PA7 | **SPI1_MOSI** | LSM6DS3TR-C | SPI1 | Output |
| PB13 | **SPI2_SCK** | DWM3000 ×4 (shared) | SPI2 | Output |
| PB14 | **SPI2_MISO** | DWM3000 ×4 (shared) | SPI2 | Input |
| PB15 | **SPI2_MOSI** | DWM3000 ×4 (shared) | SPI2 | Output |
| PC6 | CS (Top) | DWM3000 #1 | — | Output |
| PC7 | RST (Top) | DWM3000 #1 | — | Output |
| PC8 | IRQ (Top) | DWM3000 #1 | — | Input |
| PC9 | CS (Left) | DWM3000 #2 | — | Output |
| PC10 | RST (Left) | DWM3000 #2 | — | Output |
| PC11 | IRQ (Left) | DWM3000 #2 | — | Input |
| PC12 | CS (Right) | DWM3000 #3 | — | Output |
| PD2 | RST (Right) | DWM3000 #3 | — | Output |
| PA8 | IRQ (Right) | DWM3000 #3 | — | Input |
| PB6 | CS (Tool) | DWM3000 #4 | — | Output |
| PB7 | RST (Tool) | DWM3000 #4 | — | Output |
| PB8 | IRQ (Tool) | DWM3000 #4 | — | Input |
| PC0 | **Analog In** | SPW2430 MIC_TOOL (ADC direct) | ADC | Input |
| PC1 | **Analog In** | SPW2430 MIC_OUT_L (ADC direct) | ADC | Input |
| PC2 | **Analog In** | SPW2430 MIC_OUT_R (ADC direct) | ADC | Input |
| PC3 | **Analog In** | SPW2430 MIC_IN_L (via pre-amp) | ADC | Input |
| PC4 | **Analog In** | SPW2430 MIC_IN_R (via pre-amp) | ADC | Input |
| PA11 | USB_DM | USB FS connector | USB | Bidirectional |
| PA12 | USB_DP | USB FS connector | USB | Bidirectional |
| PA13 | SWDIO | ST-LINK / SWD header | SWD | Bidirectional |
| PA14 | SWCLK | ST-LINK / SWD header | SWD | Input |

**Total: 28 signals used** (4 SPI1 + 3 SPI2 + 12 GPIO for CS/RST/IRQ + 5 ADC + USB 2 + SWD 2)
**Spare GPIO remains** for UART, DAC, I2S/SAI, and future ANC control signals.

### 1.2 SPI Bus Configuration

| Parameter | SPI1 (IMU) | SPI2 (UWB ×4) |
|-----------|------------|---------------|
| Mode | Mode 3 (CPOL=1, CPHA=1) | Mode 0 (CPOL=0, CPHA=0) |
| Init Clock | 4 MHz | 2 MHz |
| Operational Clock | 4 MHz | **2 MHz** (see note) |
| Max Clock | 10 MHz (sensor limit) | 45 Mbit/s (MCU), design fixes 2 MHz |
| SCK | PA5 | PB13 |
| MOSI | PA7 | PB15 |
| MISO | PA6 | PB14 |
| CS | PA4 (software CS) | Software CS per module |

> **⚠ SPI2 clock fixed at 2 MHz (tool cable length):** The Tool DWM3000 is mounted on a tool approximately **60 cm – 1 m** away from the headset. Standard SPI assumes short runs (~15 cm); at this distance, 16 MHz cannot be guaranteed for signal integrity. **Without hardware changes**, fix the SPI2 clock for UWB to **2 MHz** in firmware. This is sufficient for four-module ranging at 5 Hz (complete within ~200 ms). For hardware mitigation, use a **shielded cable** (see §3.4 #5).

### 1.3 Power Rails

| Rail | Source | Consumers |
|------|--------|-----------|
| 5V_IN | USB VBUS or external 5V input | AP2112K VIN, USB connector VBUS |
| 3.3V_MAIN | **External LDO (required)** — AP2112K-3.3 (600 mA) | MCU, IMU, DWM3000 ×4, SPW2430 ×5, MCP6002 ×1 |
| 3.3V_MCU | Branch from 3.3V_MAIN (ferrite or 0Ω link recommended) | STM32F446RCT6 VDD / VDDA, LSM6DS3TR-C |
| 3.3V_EXT | Star branch from 3.3V_MAIN | DWM3000 ×4, SPW2430 ×5, MCP6002 ×1 |
| GND | Common | All modules |

> **⚠ LDO is required and must be the main regulator:** Estimated current for 4 DWM3000 modules: 3× RX idle (~10 mA each) + 1× TX (~120 mA) = **~150 mA** (TDMA). SPW2430 ×5 ≈ 2.5 mA; MCP6002 and MCU/IMU included, AP2112K-3.3 (600 mA) provides margin. Therefore **populate AP2112K-3.3 as the board's main 3.3V regulator** and branch `3.3V_MCU` and `3.3V_EXT` from its output.
>
> **STM32 power layout requirements:** For the LQFP-64 package the `VCAP_2` pad is not present and only `VCAP1` exists. Use `VCAP1` as the MCU internal regulator decoupling only — do NOT use the VCAP pin as a general 3.3V rail. Place a single **2.2 µF low-ESR** capacitor from VCAP1 to GND. Place VDDA/VSSA decoupling close to the analog pins (100 nF + 1 µF). Note: PA11, PA12, PB14, PB15 belong to the USB power domain in the datasheet, but on LQFP-64 the `VDDUSB` pin is not external and is internally connected to VDD — no separate `VDDUSB` net is required.

---

## 2. Schematics

### 2.1 IMU — LSM6DS3TR-C (SPI)

```
                    STM32F446RCT6                  LSM6DS3TR-C (LGA-14L)
                   ┌─────────────┐                ┌──────────────┐
                   │             │                │              │
         PA5 ──────│ SPI1 SCK    │────────────────│ SCL (Pin 13) │
         PA7 ──────│ SPI1 MOSI   │────────────────│ SDA (Pin 14) │
         PA6 ──────│ SPI1 MISO   │────────────────│ SDO (Pin 1)  │
         PA4 ──────│ IMU_CS      │────────────────│ CS  (Pin 12) │
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

**LSM6DS3TR-C pin summary (LGA-14L):**

| Pin # | Name | Connect To | Notes |
|-------|------|-----------|-------|
| 1 | SDO/SA0 | STM32 PA6 (SPI1_MISO) | SPI data out / I2C address LSB |
| 2 | SDx | GND | Mode 1 (aux I2C unused), tie to GND or VDDIO |
| 3 | SCx | GND | Mode 1 (aux I2C unused), tie to GND or VDDIO |
| 4 | INT1 | N/C | Interrupt unused |
| 5 | VDDIO | 3.3V | I/O power, 100nF decap |
| 6 | GND | GND |
| 7 | GND | GND |
| 8 | VDD | 3.3V | Core power, 100nF decap |
| 9 | INT2 | N/C | Interrupt unused |
| 10 | NC | N/C | Leave unconnected, solder pad only |
| 11 | NC | N/C | Leave unconnected, solder pad only |
| 12 | CS | STM32 PA4 (GPIO) | Active low (0 = SPI mode) |
| 13 | SCL | STM32 PA5 (SPI1_SCK) | SPI clock |
| 14 | SDA | STM32 PA7 (SPI1_MOSI) | SPI data input |

### 2.2 UWB — DWM3000 (SPI2 shared bus, ×4 modules)

```
STM32F446RCT6                  DWM3000 module

PB13 (SPI2 SCK)  ────────────> SPICLK   (Pin 20)
PB15 (SPI2 MOSI) ────────────> SPIMOSI  (Pin 18)
PB14 (SPI2 MISO) <──────────── SPIMISO  (Pin 19)

CS GPIO            ──────────> SPICSn   (Pin 17)
RST GPIO           ──────────> RSTn     (Pin 3)
IRQ GPIO           <────────── IRQ/GPIO8(Pin 22)

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

**Per-module CS/RST/IRQ assignment:**

| Module | Role | CS Pin | RST Pin | IRQ Pin |
|--------|------|--------|---------|---------|
| **T** (Top) | Headset Initiator | PC6 | PC7 | PC8 |
| **L** (Left) | Headset Initiator | PC9 | PC10 | PC11 |
| **R** (Right) | Headset Initiator | PC12 | PD2 | PA8 |
| **Tool** (Fixed) | Responder | PB6 | PB7 | PB8 |

**DWM3000 pin summary (24-pin side-castellated, 23×13 mm):**

| Pin # | Name | Connect To | Notes |
|-------|------|-----------|-------|
| 1 | GPIO1 | N/C |
| 2 | WAKEUP | GND | Sleep unused, tie low |
| 3 | RSTn | STM32 GPIO (per module) | Active low. External 10kΩ pull-up to 3.3V |
| 4–8 | — | — |
| 9 | VSS | GND | **Must connect** |
| 10 | VDD3V3 | 3.3V | Module internal regulator, 100nF decap |
| 11 | VSS | GND | **Must connect** |
| 12–16 | GPIO2–6 | N/C | Unused |
| 17 | SPICSn | STM32 GPIO (per module) | Active low |
| 18 | SPIMOSI | PB15 (SPI2 bus) |
| 19 | SPIMISO | PB14 (SPI2 bus) |
| 20 | SPICLK | PB13 (SPI2 bus) |
| 21 | EXTON | N/C |
| 22 | IRQ/GPIO8 | STM32 GPIO (per module) | **100kΩ pull-down required** |
| 23 | VSS | GND | **Must connect** |
| 24 | VDD3V3 | 3.3V | Same as Pin 10 |
| 25 | VSS | GND | **Must connect** |

### 2.3 MEMS Microphones — SPW2430 ×5 (Analog)

Five Adafruit SPW2430 MEMS microphone breakouts are used. The **three external microphones** are wired directly to the ADC (no pre-amp); the **two internal calibration microphones** pass through a pre-amp (§2.4, Gain = 21) before connecting to the STM32 ADC.

**Microphone assignment table:**

| Mic ID | Role | Location | Path | ADC Pin | APC |
|----------|------|------|------|---------|-----|
| MIC_TOOL | External | Tool (wired) | **ADC direct** (LPF only) | PC0 | — |
| MIC_OUT_L | External | Headset left outside | **ADC direct** (LPF only) | PC1 | — |
| MIC_OUT_R | External | Headset right outside | **ADC direct** (LPF only) | PC2 | — |
| MIC_IN_L | Calibration | Headset left inside | **Pre-amp §2.4** (Gain=21) | PC3 | **APC input** |
| MIC_IN_R | Calibration | Headset right inside | **Pre-amp §2.4** (Gain=21) | PC4 | **APC input** |

**SPW2430 breakout connections (same for all 5):**

| Breakout Pad | Connect To | Notes |
|-------------|-----------|-------|
| VCC | 3.3V | 1.5V–3.6V, use 3.3V |
| GND | GND |
| OUT | → external: R_LP → ADC node, C_LP is ADC node→GND | or → internal: Pre-amp → R_LP → ADC node, C_LP is ADC node→GND |

**Signal paths:**

```
External microphones (MIC_TOOL, MIC_OUT_L, MIC_OUT_R) — no pre-amp, up to ~110 dB SPL:
  SPW2430.OUT → [R_LP 1.5kΩ] ─┬→ STM32 ADC
                               └→ [C_LP 10nF] → GND

Calibration microphones (MIC_IN_L, MIC_IN_R) — pre-amp Gain=21, 60–85 dB inside earmuff:
  SPW2430.OUT → [Pre-amp §2.4] ─┬→ APC circuit input
                                 └→ [R_LP 1.5kΩ] ─┬→ STM32 ADC
                                                    └→ [C_LP 10nF] → GND
```

> **Design rationale:** External mics must capture industrial noise up to ~110 dB SPL without clipping. Removing the Gain=21 pre-amp raises ADC clipping threshold from ~90 dB to **~112 dB**. Internal calibration mics see ~60–85 dB due to earmuff attenuation (~20–30 dB), so keep Gain=21 to maximize ADC resolution.

> **MIC_TOOL wiring:** The tool DWM3000 and MIC_TOOL are wired (~60 cm–1 m). Wire bundle = SPI2 3 lines + CS + RST + IRQ + **Mic OUT** + VCC + GND = **9 wires**. Use **CAT6 shielded ethernet** or shielded multi-core cable. Recommended pairing focuses on return-path pairing: `SCK↔GND` primary, `MOSI↔GND` and `MISO↔GND` if possible. SPI2 clock fixed at 2 MHz for SI (§1.2).

> **Calibration mic branching:** Pre-amp outputs for MIC_IN_L/R must split to APC before the anti-alias LPF.

### 2.4 Pre-Amplifier Circuit — MCP6002 (×1, internal mics only)

The SPW2430 analog output level is millivolts, so a pre-amp is required to fully use the STM32 12-bit ADC (0–3.3V).

**Design specs:**

| Parameter | Value | Notes |
|-----------|-------|-------|
| Topology | Non-inverting amplifier | High-impedance input |
| Gain | 21× (26.4 dB) | 1 + R_F/R_G = 1 + 20kΩ/1kΩ |
| Input coupling | AC (1 µF) | Removes DC bias |
| Bias point | VCC/2 = 1.65V | 100kΩ/100kΩ divider |
| Anti-alias LPF | 10.6 kHz, 1st-order RC | 1.5kΩ + 10nF |
| Op-amp | MCP6002 (Microchip) | Dual, RRIO, 1 MHz GBW, SOIC-8 |
| Supply | 3.3V single-rail |
| Channels | 2 (1× MCP6002 dual) |

**Single-channel schematic (repeat ×2):**

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
        ├─────────[R_F] 20kΩ────────────────────────┘ (Feedback from output)
        │
      [R_G] 1kΩ
        │
      [C_G] 10uF
        │
       GND


    ADC anti-alias LPF:

    MCP6002.OUT ----[R_LP 1.5kΩ]----o----> STM32 ADC
                          |
                        [C_LP] 10nF
                          |
                         GND
```

> **Note:** MCP6002 power pins omitted in ASCII; actual wiring: VDD = Pin 8 → 3.3V_EXT, VSS = Pin 4 → GND, with a 100nF decoupling cap near VDD.

**Gain = 1 + R_F / R_G = 1 + 20kΩ / 1kΩ = 21 (26.4 dB)**

**Operating ranges (reference):**

| SPL (dB) | Environment | SPW2430 Vrms | Amplified Vrms | ADC Range (V) | Status |
|----------|-------------|--------------|----------------|---------------|--------|
| 60 | Quiet room | 2.5 mV | 53 mV | 1.58–1.73 | ✓ OK |
| 70 | Conversation | 8.0 mV | 168 mV | 1.41–1.89 | ✓ OK |
| 85 | Factory noise | 44.8 mV | 941 mV | 0.32–2.98 | ✓ OK |
| 90 | Loud noise | 79.6 mV | 1.67 V | clips | ⚠ soft clip |
| 94 | Reference 1 Pa | 126 mV | 2.65 V | clips | ⚠ |

> **Design target:** Use ~80% of ADC range at 85 dB SPL. Above 90 dB the op-amp output soft-clips but normal ANC range (60–85 dB) is undistorted.

**MCP6002 channel assignment (SOIC-8) — internal mics only:**

| IC | Ch A (Pin 1 out) | Ch B (Pin 7 out) |
|----|------------------|------------------|
| U1 | MIC_IN_L → PC3 | MIC_IN_R → PC4 |

**Pre-amp parts list (internal mic 2ch):**

| Component | Value | Package | Qty | Notes |
|-----------|-------|---------|-----|-------|
| MCP6002-I/SN | Dual Op-Amp | SOIC-8 | 1 | Rail-to-rail I/O, 1.8–6V |
| C_IN | 1 µF | 0402 MLCC | 2 | AC coupling, X7R |
| R_B1, R_B2 | 100 kΩ | 0402 | 4 | Bias divider (2 per ch) |
| R_F | 20 kΩ | 0402 | 2 | Feedback resistor |
| R_G | 1 kΩ | 0402 | 2 | Ground ref resistor |
| C_G | 10 µF | 0603 MLCC | 2 | DC blocking to GND (1 per ch) |
| R_LP | 1.5 kΩ | 0402 | 5 | Anti-alias LPF (5ch) |
| C_LP | 10 nF | 0402 MLCC | 5 | Anti-alias, fc ≈ 10.6 kHz |
| C_BYP | 100 nF | 0402 MLCC | 1 | Op-amp VDD decoupling |

### 2.5 PCB Design Notes

1. **Decoupling**: Place 100nF MLCCs near each VDD pin (IMU: VDD + VDDIO each, DWM3000: each module VDD3V3).
2. **SPI2 bus**: Four DWM3000 modules share MISO/MOSI/SCK. Keep traces short and matched.
3. **IRQ pull-down**: Each DWM3000 IRQ pin requires 100kΩ pull-down (per module datasheet).
4. **RSTn pull-up**: Each DWM3000 RSTn line needs 10kΩ pull-up to 3.3V (open-drain output).
5. **GND vias**: Provide sufficient ground vias under each DWM3000 module (affects RF performance).
6. **UWB antenna keepout**: Remove copper under DWM3000 antenna area (module datasheet §5).
7. **STM32 basic circuits**: `BOOT0` is held low by 100kΩ to boot from Flash. If USB DFU is required, add a jumper/testpad/button to drive `BOOT0=High`. `NRST` should have a 10kΩ pull-up and reset button. Expose an SWD header with `PA13 (SWDIO)`, `PA14 (SWCLK)`, `NRST`, `3.3V`, `GND`.
8. **STM32 clock/stability**: For USB FS and stable PLL operation at high frequencies, mount an **8 MHz HSE crystal + two load caps** on `PH0/PH1`. For LQFP-64, place **2.2 µF low-ESR** on `VCAP1` to GND.
9. **Power (mandatory)**: External 3.3V LDO is the board's main regulator (§1.3). Decouple VDDA/VSSA near analog pins. Consider star routing or ferrite bead separation between `3.3V_MCU` and `3.3V_EXT`.
10. **Microphone routing**: Keep SPW2430 analog traces away from SPI/digital traces. Place anti-alias RC near STM32 ADC pins.
11. **USB connector**: Connect `PA11/PA12` to the USB connector if USB telemetry/DFU is used. On LQFP-64 `VDDUSB` is internal to VDD; no separate `VDDUSB` net is necessary. For USB-C receptacles, provide CC resistors (5.1kΩ Rd to GND) and ESD protection.
12. **Tool cable connector**: Provide a robust connector at the PCB edge for the 9-wire tool cable (RJ45, JST, or sturdy right-angle header).

---

## 3. Sensor Placement (Physical Positions)

### 3.1 Coordinate System

```
        +Z (up)
         │
         │
         │    +Y (forward, user line-of-sight)
         │   ╱
         │  ╱
         │ ╱
         └──────── +X (right)
     
     Origin = headband center (top of head)
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
                    │ [IMU] + [STM32F446 Main PCB]  │
                    │     (at Origin,               │
                    │      headband center)         │
                    │                               │
                    └─── Back (rear of head) ───────┘
```

### 3.3 Individual Sensor Positions

| Sensor | Position (body frame) | Physical placement | Mounting |
|------|-------------------|-----------------|----------|
| **IMU** (LSM6DS3TR-C) | (0, 0, 0) — Origin | Headband center, soldered on main PCB | PCB onboard |
| **STM32F446RCT6 Main PCB** | (0, 0, 0) — Origin | Headband center, main PCB assembly | PCB onboard |
| **UWB T** (DWM3000 #1) | (0, 0, +10cm) | Top of head, upper headband | FPC or wire connection |
| **UWB L** (DWM3000 #2) | (−8cm, 0, 0) | Above left ear | FPC or wire connection |
| **UWB R** (DWM3000 #3) | (+8cm, 0, 0) | Above right ear | FPC or wire connection |
| **UWB Tool** (DWM3000 #4) | World: (0, 1.5m, 0.8m) | Fixed to tool | Wired connection (SPI2 + GPIO) |
| **MIC_TOOL** (SPW2430) | World: same as Tool | Mounted on tool with DWM3000 | Wired → LPF → PC0 (ADC direct) |
| **MIC_OUT_L** (SPW2430) | Headset left outside | Left earmuff outside | Wire → LPF → PC1 (ADC direct) |
| **MIC_OUT_R** (SPW2430) | Headset right outside | Right earmuff outside | Wire → LPF → PC2 (ADC direct) |
| **MIC_IN_L** (SPW2430) | Headset left inside | Inside left earmuff, near ear | Wire → pre-amp → LPF → PC3 + APC |
| **MIC_IN_R** (SPW2430) | Headset right inside | Inside right earmuff, near ear | Wire → pre-amp → LPF → PC4 + APC |

### 3.4 Placement Constraints

1. **UWB T/L/R must face different directions:** Place the three DWM3000 antennas near-orthogonally to minimize DOP.
2. **IMU near rotation center:** Place at headband center to reduce lever-arm effects.
3. **UWB antenna LOS:** Place antennas outward so they are not blocked by the head/helmet.
4. **Tool + MIC_TOOL fixed together:** Tool DWM3000 and MIC_TOOL mounted together on tool and wired to headset main PCB.
5. **Tool wire bundle (~1 m):** 9-wire cable between headset and tool (SPI2 3 lines + CS + RST + IRQ + Mic OUT + VCC + GND). Use **CAT6 shielded (STP) ethernet** or shielded multi-core cable (4 twisted pairs + drain). Recommended pairing uses return-path-centric pairing: `SCK↔GND` priority, then `MOSI↔GND`, `MISO↔GND`. Avoid pairing two active digital signals together (e.g., MOSI↔MISO or CS↔IRQ). SPI2 clock is **2 MHz fixed** (§1.2).
6. **Anti-alias RC placement:** Place R_LP + C_LP near STM32 ADC pins and keep microphone analog routing away from SPI/digital traces.

> **Caution — tool wire length:** For SPI2 at 16 MHz, keep cable under ~15 cm. For longer runs, reduce SPI clock to 2 MHz or consider a local MCU at the tool.

---

## 4. DigiKey Part Numbers

| Component | Manufacturer Part Number | DigiKey Part Number | Qty | Description |
|-----------|-------------------------|---------------------|-----|-------------|
| **MCU** | STM32F446RCT6 | — (verify latest DigiKey PN before ordering) | 1 | STMicro ARM Cortex-M4F, 180 MHz, LQFP-64, 256KB Flash |
| **UWB Module** | DWM3000 | DWM3000-ND | 4 | Qorvo UWB transceiver module, 23×13mm, side-castellated |
| **IMU** | LSM6DS3TR-C | 497-17232-1-ND | 1 | STMicro 6-axis IMU, LGA-14L |
| **HSE Crystal (recommended)** | 8 MHz crystal | — (generic) | 1 | For STM32 PLL / USB stability |
| **HSE Load Cap** | 18–22 pF 0402 C0G/NP0 | — (generic) | 2 | Crystal load caps |
| **VCAP Capacitor** | 2.2 µF 0603 low-ESR | — (generic) | 1 | STM32 LQFP-64 `VCAP1` required |
| **BOOT0 Pull-down** | 100 kΩ 0402 | — (generic) | 1 | Keeps Flash boot by default |
| **BOOT0 Access** | Test pad / jumper / tact switch | — (generic) | 1 | To assert BOOT0 = High for USB DFU |
| **NRST Pull-up** | 10 kΩ 0402 | — (generic) | 1 | MCU reset network |
| **SWD Header** | Cortex Debug 10-pin 1.27 mm | — (generic) | 1 | ST-LINK connection |
| **USB Connector** | USB-C receptacle or Micro-USB B | — (generic) | 1 | USB FS telemetry / DFU |
| **Decoupling Cap** | 100nF 0402 MLCC | — (generic) | 6+ | One per VDD pin |
| **IRQ Pull-down** | 100kΩ 0402 | — (generic) | 4 | DWM3000 IRQ pins |
| **RST Pull-up** | 10kΩ 0402 | — (generic) | 4 | DWM3000 RSTn pins |
| **MEMS Microphone** | Adafruit SPW2430 | 1528-1396-ND | **5** | SPW2430 breakout, Knowles SPW2430HR5H-B |
| **Pre-amp Op-Amp** | MCP6002-I/SN | MCP6002-I/SN-ND | **1** | Dual RRIO op-amp, SOIC-8 |
| **Coupling Cap (C_IN)** | 1 µF 0402 X7R | — (generic) | 2 | AC coupling (internal mics) |
| **Bias Resistor (R_B)** | 100 kΩ 0402 | — (generic) | 4 | Bias divider |
| **Gain Resistor (R_F)** | 20 kΩ 0402 | — (generic) | 2 | Feedback resistor |
| **Gain Resistor (R_G)** | 1 kΩ 0402 | — (generic) | 2 | Ground-ref resistor |
| **Ground Capacitor (C_G)** | 10 µF 0603 X5R/X7R | — (generic) | 2 | DC blocking to GND |
| **LPF Resistor (R_LP)** | 1.5 kΩ 0402 | — (generic) | 5 | Anti-alias filter |
| **LPF Cap (C_LP)** | 10 nF 0402 X7R | — (generic) | 5 | Anti-alias filter |
| **3.3V LDO (required)** | AP2112K-3.3TRG1 | AP2112K-3.3TRG1DICT-ND | 1 | 600mA LDO, SOT-23-5 |

> **Note:** Verify DigiKey PN for STM32 MCU, USB connector, crystal, and SWD header before ordering; part numbers change with stock.

### 4.1 Part Details

**STM32F446RCT6**
- MPN: `STM32F446RCT6`
- Package: LQFP-64, 10 × 10 mm, 0.5 mm pitch
- MCU: ARM Cortex-M4 + DSP + single-precision FPU @ 180 MHz
- Flash: 256 KB, SRAM: 128 KB
- Supply: 1.7V–3.6V (design uses 3.3V)
- Peripherals used: SPI1 (IMU), SPI2 (UWB), 5× ADC inputs, USB FS, SWD
- **Required board circuits:** `VCAP1` 2.2 µF ×1, `BOOT0` pull-down, `NRST` pull-up, SWD header

**DWM3000 (Qorvo)**
- MPN: `DWM3000`
- Package: 24-pin side-castellated module, 23 × 13 × 2.9 mm
- Internal IC: DW3110 (DW3000 family)
- Supply: 2.5V–3.6V (VDD3V3), internal 1.8V regulator included
- UWB channels: 5 (6.5 GHz), 9 (8 GHz)
- Onboard antenna included

**Adafruit SPW2430 MEMS Microphone Breakout**
- Adafruit #: 2716
- MEMS Mic: Knowles SPW2430HR5H-B
- Sensitivity: −18 dBV/Pa @ 94 dB SPL (126 mVrms/Pa)
- SNR: 63 dB (A-weighted)
- Output: Analog, ~VCC/2 (~1.65V)
- Supply: 1.5V–3.6V (use 3.3V)
- Breakout size: 14 × 10 mm
- Qty: **5** (MIC_TOOL, MIC_OUT_L/R, MIC_IN_L/R)

**MCP6002-I/SN (Microchip) — Pre-amp**
- Package: SOIC-8 (dual op-amp)
- Supply: 1.8V–6.0V (3.3V single-rail)
- RRIO, GBW 1 MHz
- Qty: **1** (MIC_IN_L + MIC_IN_R)

**LSM6DS3TR-C (STMicroelectronics)**
- MPN: `LSM6DS3TR-C`
- Package: LGA-14L, 2.5 × 3.0 × 0.83 mm
- Supply: 1.71V–3.6V
- Accel/Gyro ranges per design

---

## Quick Reference: Net List

```
NET: IMU_SPI1_SCK   PA5   ↔  IMU.SCL
NET: IMU_SPI1_MOSI  PA7   ↔  IMU.SDA
NET: IMU_SPI1_MISO  PA6   ↔  IMU.SDO
NET: IMU_CS         PA4   ↔  IMU.CS

NET: UWB_SPI2_SCK   PB13  ↔  UWB_T.SPICLK, UWB_L.SPICLK, UWB_R.SPICLK, UWB_TOOL.SPICLK
NET: UWB_SPI2_MOSI  PB15  ↔  UWB_T.SPIMOSI, UWB_L.SPIMOSI, UWB_R.SPIMOSI, UWB_TOOL.SPIMOSI
NET: UWB_SPI2_MISO  PB14  ↔  UWB_T.SPIMISO, UWB_L.SPIMISO, UWB_R.SPIMISO, UWB_TOOL.SPIMISO

NET: UWB_T_CS       PC6   ↔  UWB_T.SPICSn
NET: UWB_T_RST      PC7   ↔  UWB_T.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_T_IRQ      PC8   ↔  UWB_T.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_L_CS       PC9   ↔  UWB_L.SPICSn
NET: UWB_L_RST      PC10  ↔  UWB_L.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_L_IRQ      PC11  ↔  UWB_L.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_R_CS       PC12  ↔  UWB_R.SPICSn
NET: UWB_R_RST      PD2   ↔  UWB_R.RSTn     (+ 10kΩ pull-up to 3.3V)
NET: UWB_R_IRQ      PA8   ↔  UWB_R.IRQ      (+ 100kΩ pull-down to GND)

NET: UWB_TOOL_CS    PB6   ↔  UWB_TOOL.SPICSn
NET: UWB_TOOL_RST   PB7   ↔  UWB_TOOL.RSTn  (+ 10kΩ pull-up to 3.3V)
NET: UWB_TOOL_IRQ   PB8   ↔  UWB_TOOL.IRQ   (+ 100kΩ pull-down to GND)

NET: MIC_TOOL       PC0   ↔  SPW2430.OUT → R_LP/C_LP           (direct, shielded wire)
NET: MIC_OUT_L      PC1   ↔  SPW2430.OUT → R_LP/C_LP           (direct)
NET: MIC_OUT_R      PC2   ↔  SPW2430.OUT → R_LP/C_LP           (direct)
NET: MIC_IN_L       PC3   ↔  SPW2430.OUT → U1A(pre-amp) ─┬→ R_LP/C_LP  (+ APC tee)
NET: MIC_IN_R       PC4   ↔  SPW2430.OUT → U1B(pre-amp) ─┬→ R_LP/C_LP  (+ APC tee)

NET: PREAMP_VCC     3.3V_EXT ↔  U1.VDD(8) + 100nF decap
NET: PREAMP_GND     GND      ↔  U1.VSS(4)

NET: 3V3_MAIN       AP2112K 3.3V out       ↔  STM32 VDD + VDDA + IMU + DWM3000 ×4 + SPW2430 ×5 + MCP6002 ×1
NET: 3V3_MCU        3.3V_MAIN branch       ↔  STM32 VDD / VDDA + IMU VDD + IMU VDDIO
NET: 3V3_EXT        3.3V_MAIN branch       ↔  DWM3000 ×4 VDD3V3 + SPW2430 ×5 VCC + MCP6002 ×1
NET: LDO_IN         USB VBUS / external 5V ↔  AP2112K VIN (LDO input)
NET: USB_DM         PA11 ↔ USB connector D−
NET: USB_DP         PA12 ↔ USB connector D+
NET: SWDIO          PA13 ↔ ST-LINK header
NET: SWCLK          PA14 ↔ ST-LINK header
NET: NRST           NRST ↔ reset button + SWD header
NET: BOOT0          BOOT0 ↔ 100kΩ pull-down to GND + optional jumper/test pad to 3.3V for DFU
NET: VCAP1          VCAP1 ↔ 2.2µF to GND
NET: GND            Common ground ↔ All VSS/GND/VSSA pins
```
# LEGACY DOCUMENT

This file is preserved for early prototype history only.
It is not the active hardware reference for the current integrated Teensy mainboard.

Use these files instead:

- `ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`
- `SATELLITE_BOARD_KR.md`
