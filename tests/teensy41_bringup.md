# Teensy 4.1 Bring-Up

## Build Order

1. `pio run -e teensy41_imu_only`
2. `pio run -e teensy41_uwb_detect`
3. `pio run -e teensy41_single_range`
4. `pio run -e teensy41_full_range`
5. `pio run -e teensy41_full_fusion`

## Bench Checks

### IMU Only

- boot succeeds
- IMU calibration completes
- `$STAT` shows IMU active

### UWB Detect

- all expected UWB modules initialize
- `$STAT` reports expected top/left/right/tool readiness

### Single Range

- one selected ranging link completes repeatedly
- `$UWBDBG` status codes remain stable

### Full Range

- all intended ranging links produce valid timestamps
- `$UWBTS` updates every range cycle

### Full Fusion

- `$POSE` outputs position/orientation continuously
- ESKF accepted/rejected counts in `$STAT` behave reasonably

## Known Gaps

- `IMU_R_HI` still needs measured values
- antenna delays still need measured values
- integrated audio path is not part of the current bring-up validation
