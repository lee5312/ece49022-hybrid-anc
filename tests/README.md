# Test Plan Index

This directory contains bench-level validation plans for the active Teensy-based system.

## Current Priorities

- Teensy 4.1 sensor bring-up
- UWB module detection and ranging
- IMU calibration and fusion validation
- satellite board standalone validation
- future integrated audio-path validation

## Expected Test Artifacts

- serial logs from `$POSE`, `$UWBTS`, `$UWBDBG`, `$STAT`
- measured IMU calibration values
- measured UWB antenna delay values
- satellite board pass/fail notes

## Notes

- The current repository does not yet contain finalized production ANC audio tests for the integrated codec/analog path.
- Legacy MATLAB validation remains in `simulations/` and `tests/anctest.m`.
