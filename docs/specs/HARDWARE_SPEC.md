# Hardware Specification

## Active Board Set

The active hardware baseline is the integrated `Teensy 4.1` mainboard plus three satellite boards.

### Mainboard

- `Teensy 4.1` MCU
- mainboard UWB module `U16`
- IMU on `J_IMU_1`
- satellite headers for `Tool`, `Left`, `Right`
- integrated audio ADC, DAC, and analog processing blocks

### Satellite Boards

Each satellite board contains:

- one `DWM3000TR13`
- one microphone breakout header for `Adafruit SPW2430`
- one 9-pin connector back to the mainboard

## Detailed Source Documents

- `ALTIUM_MAINBOARD_COMPLETE_FINAL_FROM_SHEET1_NET_KR.md`
- `SATELLITE_BOARD_KR.md`

## Critical Hardware Notes

- The active mainboard is not a simple `Teensy + UWB + IMU` board.
- The active mainboard includes integrated audio converter and analog blocks.
- `J_IMU_1` is the active IMU connector in the current netlist baseline.
- Tool satellite analog pin 9 is routed differently from Left/Right analog pin 9.

## Legacy Hardware Documents

- `PCB_HANDOFF_STM32.md` is kept only as historical prototype documentation.
