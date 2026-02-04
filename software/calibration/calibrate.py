#!/usr/bin/env python3
"""
SAHANC Calibration Tool
Generates H-Map from swept sine measurements
"""

import numpy as np
import serial
import time
from dataclasses import dataclass
from typing import List, Tuple
import json

@dataclass
class HMapEntry:
    distance_m: float
    angle_deg: float
    frequency_hz: float
    gain: float
    phase_deg: float

class SAHANCCalibrator:
    def __init__(self, serial_port: str = '/dev/ttyUSB0', baud_rate: int = 115200):
        self.ser = serial.Serial(serial_port, baud_rate, timeout=1)
        self.hmap: List[HMapEntry] = []
        
        # Calibration parameters
        self.freq_start = 100  # Hz
        self.freq_end = 8000   # Hz
        self.freq_steps = 50
        self.distance_points = [0.5, 1.0, 1.5, 2.0, 3.0, 5.0]  # meters
        self.angle_points = list(range(0, 360, 30))  # every 30 degrees
    
    def generate_sweep(self, duration_s: float = 2.0, sample_rate: int = 48000) -> np.ndarray:
        """Generate logarithmic frequency sweep"""
        t = np.linspace(0, duration_s, int(duration_s * sample_rate))
        sweep = np.sin(2 * np.pi * np.logspace(
            np.log10(self.freq_start),
            np.log10(self.freq_end),
            len(t)
        ) * t)
        return (sweep * 32767).astype(np.int16)
    
    def measure_transfer_function(self, input_signal: np.ndarray, 
                                   output_signal: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """Calculate transfer function H(f) = Y(f) / X(f)"""
        X = np.fft.rfft(input_signal)
        Y = np.fft.rfft(output_signal)
        
        H = Y / (X + 1e-10)  # Avoid division by zero
        
        magnitude = np.abs(H)
        phase = np.angle(H, deg=True)
        
        return magnitude, phase
    
    def calibrate_position(self, distance: float, angle: float) -> List[HMapEntry]:
        """Calibrate at a specific position"""
        print(f"Calibrating at distance={distance}m, angle={angle}°")
        
        # Send calibration command
        cmd = {'cmd': 'calibrate', 'distance': distance, 'angle': angle}
        self.ser.write(json.dumps(cmd).encode() + b'\n')
        
        # Generate and play sweep
        sweep = self.generate_sweep()
        # TODO: Send sweep to noise source speaker
        
        # Record from FDM and calibration mic
        # TODO: Receive recorded data
        time.sleep(3)  # Wait for recording
        
        # Calculate transfer function for each frequency band
        entries = []
        freqs = np.logspace(np.log10(self.freq_start), np.log10(self.freq_end), self.freq_steps)
        
        for freq in freqs:
            # TODO: Extract gain and phase at this frequency
            entry = HMapEntry(
                distance_m=distance,
                angle_deg=angle,
                frequency_hz=freq,
                gain=1.0,  # Placeholder
                phase_deg=180.0  # Placeholder
            )
            entries.append(entry)
        
        return entries
    
    def run_full_calibration(self):
        """Run calibration at all positions"""
        print("Starting full calibration...")
        print(f"Distances: {self.distance_points}")
        print(f"Angles: {self.angle_points}")
        
        for distance in self.distance_points:
            for angle in self.angle_points:
                entries = self.calibrate_position(distance, angle)
                self.hmap.extend(entries)
        
        print(f"Calibration complete. {len(self.hmap)} entries generated.")
    
    def save_hmap(self, filename: str = 'hmap.json'):
        """Save H-Map to file"""
        data = [
            {
                'd': e.distance_m,
                'a': e.angle_deg,
                'f': e.frequency_hz,
                'g': e.gain,
                'p': e.phase_deg
            }
            for e in self.hmap
        ]
        
        with open(filename, 'w') as f:
            json.dump(data, f)
        
        print(f"H-Map saved to {filename}")
    
    def export_c_header(self, filename: str = 'hmap_data.h'):
        """Export H-Map as C header for embedded use"""
        # Organize into 3D array [distance][angle][frequency]
        # TODO: Implement C header generation
        pass


if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='SAHANC Calibration Tool')
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--output', default='hmap.json', help='Output file')
    args = parser.parse_args()
    
    calibrator = SAHANCCalibrator(serial_port=args.port)
    
    print("SAHANC Calibration Tool")
    print("=" * 40)
    print("Commands:")
    print("  1. Run full calibration")
    print("  2. Calibrate single position")
    print("  3. Load existing H-Map")
    print("  4. Save H-Map")
    print("  q. Quit")
    
    while True:
        choice = input("\nEnter command: ").strip()
        
        if choice == '1':
            calibrator.run_full_calibration()
        elif choice == '2':
            d = float(input("Distance (m): "))
            a = float(input("Angle (deg): "))
            calibrator.calibrate_position(d, a)
        elif choice == '4':
            calibrator.save_hmap(args.output)
        elif choice.lower() == 'q':
            break
