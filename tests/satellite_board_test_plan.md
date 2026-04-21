# Satellite Board Test Plan

## Goal

Validate each satellite board before full mainboard integration.

## Per-Board Checks

1. verify no short between `3.3V` and `GND`
2. verify `RSTn` pull-up behavior
3. verify `IRQ` pull-down behavior
4. verify stable DWM3000 device ID read
5. verify microphone continuity on connector pin 9

## Two-Board Checks

1. share SPI clock/MOSI/MISO
2. separate `CS`, `RST`, `IRQ`
3. confirm both boards probe cleanly
4. confirm one board can initiate and the other can respond
5. confirm range decreases/increases with physical movement

## Notes

- Tool, Left, and Right boards share the same basic UWB+mic architecture.
- The mainboard source-of-truth document must still be used to interpret how each board's analog pin 9 is consumed after integration.
