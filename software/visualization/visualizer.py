#!/usr/bin/env python3
"""
visualizer.py — Real-time 3D visualization for ECE 49022 Dae
──────────────────────────────────────────────────────────────
Reads $POSE serial packets from the Proton board and renders:
  - Left panel:  3D orientation box (rotates with roll/pitch/yaw)
  - Right panel: world-space position trail + tag locations

Works for both IMU-only mode (position = 0) and full ESKF mode.

Serial format:
    $POSE,x,y,z,roll_deg,pitch_deg,yaw_deg,r_t,r_l,r_r\n

Usage:
    python visualizer.py               # auto-detect COM port
    python visualizer.py --port COM5   # specific port
    python visualizer.py --port /dev/ttyACM0 --baud 115200

Dependencies:
    pip install pyserial matplotlib numpy
"""

import argparse
import sys
import time
import threading
from collections import deque

import numpy as np
import matplotlib
matplotlib.use("Qt5Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial not installed.  Run: pip install pyserial")
    sys.exit(1)

# ════════════════════════════════════════════════════════════════
#  Configuration
# ════════════════════════════════════════════════════════════════

# Tag world-frame positions (must match firmware constants, in cm)
TAG_POSITIONS = {
    "T": np.array([0.0, 150.0, 150.0]),   # Top
    "L": np.array([-100.0, 150.0, 50.0]),  # Left
    "R": np.array([100.0, 150.0, 50.0]),   # Right
}

TRAIL_LENGTH = 200       # number of past positions to show
AXIS_LIMIT   = 150.0     # centimetres, half-range for position panel
FPS          = 30        # animation frame rate

# 3D box dimensions (headset-like proportions, in centimetres)
BOX_W = 18.0   # X (ear to ear)  — 18 cm
BOX_H =  8.0   # Y (top to bottom, up) — 8 cm
BOX_D = 12.0   # Z (front to back) — 12 cm

# ════════════════════════════════════════════════════════════════
#  Shared state (serial reader thread → plot)
# ════════════════════════════════════════════════════════════════

class PoseData:
    def __init__(self):
        self.lock = threading.Lock()
        self.pos = np.zeros(3)
        self.pos_offset = np.zeros(3)  # subtracted from raw pos for reset
        self.euler_deg = np.zeros(3)  # roll, pitch, yaw
        self.ranges = np.zeros(3)     # r_t, r_l, r_r
        self.trail = deque(maxlen=TRAIL_LENGTH)
        self.updated = False
        self.pkt_count = 0
        self.fps = 0.0

pose = PoseData()

# ════════════════════════════════════════════════════════════════
#  Serial reader thread
# ════════════════════════════════════════════════════════════════

def serial_reader(port: str, baud: int):
    """Continuously reads serial data and updates pose."""
    count = 0
    t0 = time.time()

    while True:
        try:
            ser = serial.Serial(port, baud, timeout=1.0)
            print(f"[Serial] Connected to {port} @ {baud}")
        except serial.SerialException as e:
            print(f"[Serial] Failed to open {port}: {e}. Retrying in 2s...")
            time.sleep(2)
            continue

        try:
            while True:
                try:
                    line = ser.readline().decode("ascii", errors="ignore").strip()
                except Exception:
                    time.sleep(0.01)
                    continue

                if not line.startswith("$POSE,"):
                    continue

                parts = line[6:].split(",")
                if len(parts) < 9:
                    continue

                try:
                    vals = [float(v) for v in parts[:9]]
                except ValueError:
                    continue

                with pose.lock:
                    raw = np.array([vals[0]*100, vals[1]*100, vals[2]*100])  # m → cm
                    pose.pos[:] = raw - pose.pos_offset
                    pose.euler_deg[:] = vals[3:6]
                    pose.ranges[:] = [vals[6]*100, vals[7]*100, vals[8]*100]  # m → cm
                    pose.trail.append(pose.pos.copy())
                    pose.updated = True
                    pose.pkt_count += 1

                count += 1
                elapsed = time.time() - t0
                if elapsed >= 1.0:
                    pose.fps = count / elapsed
                    count = 0
                    t0 = time.time()

        except serial.SerialException:
            print("[Serial] Disconnected. Retrying in 2s...")
            time.sleep(2)

# ════════════════════════════════════════════════════════════════
#  Rotation helper (Euler ZYX → rotation matrix)
# ════════════════════════════════════════════════════════════════

def euler_to_rotmat(roll_deg, pitch_deg, yaw_deg):
    r = np.radians(roll_deg)
    p = np.radians(pitch_deg)
    y = np.radians(yaw_deg)

    cr, sr = np.cos(r), np.sin(r)
    cp, sp = np.cos(p), np.sin(p)
    cy, sy = np.cos(y), np.sin(y)

    R = np.array([
        [cy*cp, cy*sp*sr - sy*cr, cy*sp*cr + sy*sr],
        [sy*cp, sy*sp*sr + cy*cr, sy*sp*cr - cy*sr],
        [-sp,   cp*sr,            cp*cr           ],
    ])
    return R

# ════════════════════════════════════════════════════════════════
#  3D box geometry
# ════════════════════════════════════════════════════════════════

def make_box_verts(w, h, d):
    """8 vertices of a box centered at origin."""
    hw, hh, hd = w/2, h/2, d/2
    return np.array([
        [-hw, -hh, -hd],  # 0 back-bottom-left
        [ hw, -hh, -hd],  # 1 back-bottom-right
        [ hw,  hh, -hd],  # 2 back-top-right
        [-hw,  hh, -hd],  # 3 back-top-left
        [-hw, -hh,  hd],  # 4 front-bottom-left
        [ hw, -hh,  hd],  # 5 front-bottom-right
        [ hw,  hh,  hd],  # 6 front-top-right
        [-hw,  hh,  hd],  # 7 front-top-left
    ])

# 6 faces (quad index lists)
BOX_FACES = [
    [0, 1, 2, 3],  # back   (−Z)
    [4, 5, 6, 7],  # front  (+Z)
    [0, 4, 7, 3],  # left   (−X)
    [1, 5, 6, 2],  # right  (+X)
    [3, 2, 6, 7],  # top    (+Y)
    [0, 1, 5, 4],  # bottom (−Y)
]

FACE_COLORS = [
    (0.35, 0.35, 0.35, 0.7),   # back   — dark gray
    (0.15, 0.55, 0.85, 0.8),   # front  — blue (forward)
    (0.80, 0.25, 0.25, 0.7),   # left   — red
    (0.25, 0.75, 0.25, 0.7),   # right  — green
    (0.85, 0.85, 0.20, 0.7),   # top    — yellow
    (0.50, 0.50, 0.50, 0.5),   # bottom — gray
]

BASE_VERTS = make_box_verts(BOX_W, BOX_H, BOX_D)

# ════════════════════════════════════════════════════════════════
#  Plot setup — two side-by-side 3D panels
# ════════════════════════════════════════════════════════════════

fig = plt.figure(figsize=(14, 6))
fig.canvas.manager.set_window_title("ECE 49022 — Dae Visualizer")
fig.patch.set_facecolor("#1a1a2e")

# Left panel: orientation box
ax_ori = fig.add_subplot(121, projection="3d", facecolor="#16213e")
ax_ori.set_title("Orientation", color="white", fontsize=13, pad=10)

# Right panel: position trail
ax_pos = fig.add_subplot(122, projection="3d", facecolor="#16213e")
ax_pos.set_title("Position Trail", color="white", fontsize=13, pad=10)

def style_axis(ax, limit, labels=("X", "Y", "Z")):
    ax.set_xlim(-limit, limit)
    ax.set_ylim(-limit, limit)
    ax.set_zlim(-limit, limit)
    ax.set_xlabel(labels[0], color="gray")
    ax.set_ylabel(labels[1], color="gray")
    ax.set_zlabel(labels[2], color="gray")
    ax.tick_params(colors="gray", labelsize=7)
    ax.xaxis.pane.fill = False
    ax.yaxis.pane.fill = False
    ax.zaxis.pane.fill = False
    ax.xaxis.pane.set_edgecolor("#333")
    ax.yaxis.pane.set_edgecolor("#333")
    ax.zaxis.pane.set_edgecolor("#333")

style_axis(ax_ori, 18.0)
style_axis(ax_pos, AXIS_LIMIT, ("X (cm)", "Y (cm)", "Z (cm)"))

# ── Static elements on position panel ────────────────────────
# Tag markers
for name, tpos in TAG_POSITIONS.items():
    ax_pos.plot([tpos[0]], [tpos[1]], [tpos[2]], "c^",
                markersize=10, label=f"Tag {name}")

headset_dot, = ax_pos.plot([], [], [], "ro", markersize=8, label="Headset")
trail_line,  = ax_pos.plot([], [], [], "r-", alpha=0.4, linewidth=1)

range_lines = []
for _ in range(3):
    ln, = ax_pos.plot([], [], [], "g--", alpha=0.3, linewidth=0.8)
    range_lines.append(ln)

ax_pos.legend(loc="upper right", fontsize=7, facecolor="#16213e",
              edgecolor="gray", labelcolor="white")

# ── Orientation axes arrows (world reference) ────────────────
for axis_dir, color in zip(np.eye(3) * 15.0, ["red", "lime", "dodgerblue"]):
    ax_ori.plot([0, axis_dir[0]], [0, axis_dir[1]], [0, axis_dir[2]],
                color=color, linewidth=1, alpha=0.3)

# ── HUD text ─────────────────────────────────────────────────
info_text = fig.text(0.01, 0.01, "", fontsize=9, family="monospace",
                     color="#cccccc", verticalalignment="bottom")

# ── Reset Position button ────────────────────────────────────
ax_btn = fig.add_axes([0.44, 0.92, 0.12, 0.05])  # [left, bottom, width, height]
btn_reset = Button(ax_btn, 'Reset Pos', color='#2a2a4a', hovercolor='#4a4a8a')
btn_reset.label.set_color('white')
btn_reset.label.set_fontsize(10)

def on_reset_pos(event):
    with pose.lock:
        pose.pos_offset[:] = pose.pos + pose.pos_offset  # new offset = current raw
        pose.pos[:] = 0.0
        pose.trail.clear()

btn_reset.on_clicked(on_reset_pos)

# ── Mutable artist lists ─────────────────────────────────────
box_collection = [None]  # Poly3DCollection for the box
orientation_arrows = []   # quiver artists on ax_ori
body_arrows_pos = []      # quiver artists on ax_pos

# ════════════════════════════════════════════════════════════════
#  Animation update
# ════════════════════════════════════════════════════════════════

def update(frame):
    with pose.lock:
        p = pose.pos.copy()
        e = pose.euler_deg.copy()
        rng = pose.ranges.copy()
        trail = list(pose.trail)
        fps = pose.fps

    R = euler_to_rotmat(e[0], e[1], e[2])

    # ──────────────────────────────────────────────────────────
    #  Left panel: orientation box
    # ──────────────────────────────────────────────────────────

    # Remove old box
    if box_collection[0] is not None:
        box_collection[0].remove()
        box_collection[0] = None

    # Remove old arrows
    for a in orientation_arrows:
        a.remove()
    orientation_arrows.clear()

    # Rotate vertices
    rotated = (R @ BASE_VERTS.T).T

    # Build face polygons
    faces = [[rotated[vi] for vi in face] for face in BOX_FACES]
    poly = Poly3DCollection(faces, linewidths=1.5, edgecolors="white")
    poly.set_facecolor(FACE_COLORS)
    ax_ori.add_collection3d(poly)
    box_collection[0] = poly

    # Forward arrow (Z axis of body)
    fwd = R @ np.array([0, 0, 1.0])
    q = ax_ori.quiver(0, 0, 0, fwd[0], fwd[1], fwd[2],
                       color="white", linewidth=2.5,
                       arrow_length_ratio=0.15, length=12.0)
    orientation_arrows.append(q)

    # Body axes
    arrow_len = 9.0
    colors_body = ["#ff6666", "#66ff66", "#6699ff"]
    for i in range(3):
        d = R[:, i] * arrow_len
        q = ax_ori.quiver(0, 0, 0, d[0], d[1], d[2],
                           color=colors_body[i], linewidth=1.5,
                           arrow_length_ratio=0.12)
        orientation_arrows.append(q)

    # ──────────────────────────────────────────────────────────
    #  Right panel: position trail
    # ──────────────────────────────────────────────────────────

    headset_dot.set_data_3d([p[0]], [p[1]], [p[2]])

    if len(trail) > 1:
        tr = np.array(trail)
        trail_line.set_data_3d(tr[:, 0], tr[:, 1], tr[:, 2])

    # Range lines (headset → each tag)
    for i, (name, tpos) in enumerate(TAG_POSITIONS.items()):
        if rng[i] > 0.001:
            range_lines[i].set_data_3d([p[0], tpos[0]],
                                        [p[1], tpos[1]],
                                        [p[2], tpos[2]])
        else:
            range_lines[i].set_data_3d([], [], [])

    # Body axes on position panel
    for a in body_arrows_pos:
        a.remove()
    body_arrows_pos.clear()

    arrow_len_pos = 15.0
    for i in range(3):
        d = R[:, i] * arrow_len_pos
        q = ax_pos.quiver(p[0], p[1], p[2], d[0], d[1], d[2],
                           color=colors_body[i], linewidth=1.5,
                           arrow_length_ratio=0.2)
        body_arrows_pos.append(q)

    # ──────────────────────────────────────────────────────────
    #  HUD info text
    # ──────────────────────────────────────────────────────────
    has_pos = np.linalg.norm(p) > 0.001
    mode = "ESKF (UWB+IMU)" if has_pos else "IMU Only"

    info_text.set_text(
        f"Mode: {mode}  |  {fps:.0f} Hz  |  "
        f"Roll={e[0]:+6.1f}°  Pitch={e[1]:+6.1f}°  Yaw={e[2]:+6.1f}°  |  "
        f"Pos=({p[0]:+.1f}, {p[1]:+.1f}, {p[2]:+.1f})cm"
    )

    return []

# ════════════════════════════════════════════════════════════════
#  Entry point
# ════════════════════════════════════════════════════════════════

def find_serial_port():
    """Auto-detect a likely serial port (RP2350 / Pico)."""
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        vid = p.vid or 0
        if vid == 0x2E8A or "pico" in desc or "rp2" in desc or "proton" in desc:
            return p.device
    ports = [p.device for p in serial.tools.list_ports.comports()]
    return ports[0] if ports else None

def main():
    parser = argparse.ArgumentParser(
        description="ECE 49022 Dae — Real-time 3D Head Tracker Visualizer")
    parser.add_argument("--port", type=str, default=None,
                        help="Serial port (e.g. COM5, /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="Baud rate (default: 115200)")
    parser.add_argument("--sim", action="store_true",
                        help="Run internal simulator (no board required)")
    args = parser.parse_args()

    # Simulator mode: spawn a thread that updates `pose` periodically
    def sim_thread(rate_hz=50):
        t = 0.0
        dt = 1.0 / rate_hz
        while True:
            # slow translational drift + oscillation
            x = 20.0 * np.sin(2.0 * np.pi * 0.05 * t)
            y = 120.0 + 5.0 * np.sin(2.0 * np.pi * 0.07 * t)
            z = 60.0 + 3.0 * np.cos(2.0 * np.pi * 0.04 * t)

            # rotational oscillations (deg)
            roll  = 18.0 * np.sin(2.0 * np.pi * 0.2 * t)
            pitch = 10.0 * np.sin(2.0 * np.pi * 0.15 * t)
            yaw   = 30.0 * np.sin(2.0 * np.pi * 0.1 * t)

            with pose.lock:
                pose.pos[:] = np.array([x, y, z])
                pose.euler_deg[:] = np.array([roll, pitch, yaw])
                # ranges unused in IMU-only sim
                pose.ranges[:] = np.array([0.0, 0.0, 0.0])
                pose.trail.append(pose.pos.copy())
                pose.updated = True
                pose.pkt_count += 1

            t += dt
            time.sleep(dt)

    if args.sim:
        print("[Visualizer] Running in SIMULATION mode (no board required)")
        sim = threading.Thread(target=sim_thread, daemon=True)
        sim.start()
    else:
        port = args.port or find_serial_port()
        if port is None:
            print("ERROR: No serial port found. Specify with --port or use --sim")
            sys.exit(1)
        print(f"[Visualizer] Using port: {port}")

        # Start serial reader thread
        reader = threading.Thread(target=serial_reader, args=(port, args.baud),
                                  daemon=True)
        reader.start()

    # Start animation
    _ani = FuncAnimation(fig, update, interval=1000 // FPS, blit=False,
                         cache_frame_data=False)
    plt.tight_layout(pad=2.0)
    plt.show()

if __name__ == "__main__":
    main()
