# Bill Of Materials
## Active Integrated Board Baseline

This file tracks the major active assemblies and ICs for the current Teensy-based system.
For exact passive-level manufacturing output, use the Altium project and its generated BOM/export artifacts.

## Mainboard Core

| Item | Qty | Notes |
|------|-----|-------|
| Teensy 4.1 | 1 | main MCU board |
| DWM3000TR13 | 1 | mainboard UWB `U16` |
| IMU breakout | 1 | connected on `J_IMU_1` |

## Satellite Boards

| Item | Qty | Notes |
|------|-----|-------|
| DWM3000TR13 | 3 | Tool, Left, Right satellite boards |
| Adafruit SPW2430 breakout | 3 | one per satellite board |
| 9-pin satellite cable/header set | 3 | one per satellite board |

## Integrated Audio And Analog Blocks

| Item | Qty | Notes |
|------|-----|-------|
| PCM1808PWR | 2 | ADC |
| PCM5102APWR | 3 | DAC |
| OPA4192IPWR | 4 | analog stages |
| OPA2192IDR | 6 | analog stages |
| OPA1656IDR | 2 | analog stages |
| AD835ARZ-REEL7 | 8 | analog multiplier stages |

## Power Components

| Item | Qty | Notes |
|------|-----|-------|
| TPS62842DGRR | 1 | power stage |
| TPS7A2033PDBVR | 1 | power stage |
| LM27761DSGR | 1 | negative rail generation |
| TPS63700DRCTG4 | 1 | rail generation |
| LT1964ES5-5#TRMPBF | 1 | negative regulator |

## Calibration / Internal Mic Headers

| Item | Qty | Notes |
|------|-----|-------|
| calibration mic header `J9` | 1 | left calibration path |
| calibration mic header `J10` | 1 | right calibration path |

## Notes

- This list reflects the active integrated `Sheet1.NET` mainboard, not the older STM32/ESP32 prototype.
- Exact passives, connector part numbers, and manufacturing outputs should come from the Altium source set and generated BOM exports.
