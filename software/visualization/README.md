# Visualization

This folder contains PC-side visualization support for Teensy telemetry.

## Current Tool

- `visualizer.py` reads serial telemetry such as `$POSE`

## Setup

```bash
pip install -r software/visualization/requirements.txt
python software/visualization/visualizer.py
```

## Notes

- The visualization tool is intended for bench validation of pose and range telemetry.
- It is not a production user interface.
