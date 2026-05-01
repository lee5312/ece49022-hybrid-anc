import argparse
import queue
import threading
import time
import tkinter as tk
from collections import deque

import serial


PARAMS = [
    ("Gain", "gain", 0.0, 24.0, 0.1, 8.0),
    ("Mask attack", "mu", 0.0, 1.0, 0.01, 0.60),
    ("Ref floor", "refloor", 0.0, 40.0, 0.5, 8.0),
    ("Eps scale", "eps", 0.0, 80.0, 1.0, 20.0),
    ("Relevance gate", "gate", 0.0, 0.30, 0.005, 0.02),
    ("Ratio start", "start", 0.0, 0.30, 0.005, 0.03),
    ("Mask strength", "strength", 0.0, 20.0, 0.25, 5.0),
    ("Mask min", "min", 0.0, 0.40, 0.01, 0.06),
    ("Release", "release", 0.0, 0.30, 0.005, 0.06),
]


class LiveTuner:
    def __init__(self, root, port, baud, decimation):
        self.root = root
        self.port = port
        self.baud = baud
        self.decimation = decimation
        self.serial = None
        self.serial_lock = threading.Lock()
        self.connected = False
        self.queue = queue.Queue()
        self.running = True
        self.samples = deque(maxlen=900)
        self.primary_hp = deque(maxlen=900)
        self.reference_hp = deque(maxlen=900)
        self.primary_raw = deque(maxlen=900)
        self.reference_raw = deque(maxlen=900)
        self.last_line = ""
        self.last_sample_time = time.time()
        self.pending = {}
        self.raw_zoom = tk.DoubleVar(value=25.0)
        self.output_zoom = tk.DoubleVar(value=18.0)
        self.last_stats = {
            "seq": 0,
            "p_env": 0,
            "r_env": 0,
            "out_env": 0,
            "updates": 0,
            "bins": 0,
        }

        self.root.title("A7_6 Output Visual Tuner")
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.build_ui()

        self.reader = threading.Thread(target=self.read_loop, daemon=True)
        self.reader.start()

        self.root.after(25, self.tick)

    def build_ui(self):
        traces = tk.Frame(self.root)
        traces.grid(row=0, column=0, columnspan=3, sticky="nsew", padx=12, pady=(12, 6))
        self.mic1_canvas = tk.Canvas(traces, width=980, height=170, bg="#101418", highlightthickness=0)
        self.mic2_canvas = tk.Canvas(traces, width=980, height=170, bg="#101418", highlightthickness=0)
        self.output_canvas = tk.Canvas(traces, width=980, height=190, bg="#101418", highlightthickness=0)
        self.mic1_canvas.grid(row=0, column=0, sticky="nsew", pady=(0, 6))
        self.mic2_canvas.grid(row=1, column=0, sticky="nsew", pady=6)
        self.output_canvas.grid(row=2, column=0, sticky="nsew", pady=(6, 0))
        traces.columnconfigure(0, weight=1)
        traces.rowconfigure(0, weight=1)
        traces.rowconfigure(1, weight=1)
        traces.rowconfigure(2, weight=1)

        self.status = tk.StringVar(value="Waiting for $OUT samples...")
        tk.Label(self.root, textvariable=self.status, anchor="w").grid(
            row=1, column=0, columnspan=3, sticky="ew", padx=12
        )

        controls = tk.Frame(self.root)
        controls.grid(row=2, column=0, sticky="nsew", padx=12, pady=8)
        tk.Scale(
            controls,
            label="Raw display zoom",
            from_=1.0,
            to=40.0,
            resolution=0.5,
            orient="horizontal",
            length=310,
            variable=self.raw_zoom,
        ).grid(row=0, column=0, sticky="ew", pady=(0, 8))
        tk.Scale(
            controls,
            label="Output display zoom",
            from_=1.0,
            to=40.0,
            resolution=0.5,
            orient="horizontal",
            length=310,
            variable=self.output_zoom,
        ).grid(row=1, column=0, sticky="ew", pady=(0, 8))
        for idx, (label, command, low, high, step, default) in enumerate(PARAMS, start=2):
            var = tk.DoubleVar(value=default)
            scale = tk.Scale(
                controls,
                label=label,
                from_=low,
                to=high,
                resolution=step,
                orient="horizontal",
                length=310,
                variable=var,
                command=lambda value, cmd=command: self.schedule_param(cmd, value),
            )
            scale.grid(row=idx, column=0, sticky="ew", pady=1)

        buttons = tk.Frame(self.root)
        buttons.grid(row=2, column=1, sticky="n", padx=12, pady=8)
        tk.Button(buttons, text="Cancel", width=14, command=lambda: self.send("speaker mode cancel")).grid(row=0, column=0, pady=3)
        tk.Button(buttons, text="Mic1 raw", width=14, command=lambda: self.send("speaker mode primary")).grid(row=1, column=0, pady=3)
        tk.Button(buttons, text="Mic2 raw", width=14, command=lambda: self.send("speaker mode reference")).grid(row=2, column=0, pady=3)
        tk.Button(buttons, text="Reset mask", width=14, command=lambda: self.send("speaker reset")).grid(row=3, column=0, pady=3)
        tk.Button(buttons, text="Status", width=14, command=lambda: self.send("speaker status")).grid(row=4, column=0, pady=3)

        self.log = tk.Text(self.root, width=54, height=18)
        self.log.grid(row=2, column=2, sticky="nsew", padx=(0, 12), pady=8)

        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(2, weight=1)
        self.root.rowconfigure(0, weight=3)
        self.root.rowconfigure(2, weight=1)

    def log_line(self, text):
        if not text:
            return
        self.log.insert("end", text + "\n")
        self.log.see("end")

    def send(self, command):
        with self.serial_lock:
            port = self.serial
            if port is None or not port.is_open:
                self.queue.put(("log", f"not connected, skipped: {command}"))
                return
            try:
                port.write((command + "\r\n").encode("ascii"))
            except (serial.SerialException, OSError) as exc:
                self.queue.put(("log", f"serial write failed: {exc}"))
                self.close_serial_locked()

    def send_locked(self, command):
        if self.serial is None or not self.serial.is_open:
            return
        try:
            self.serial.write((command + "\r\n").encode("ascii"))
        except (serial.SerialException, OSError) as exc:
            self.queue.put(("log", f"serial write failed: {exc}"))
            self.close_serial_locked()

    def close_serial_locked(self):
        if self.serial is not None:
            try:
                self.serial.close()
            except serial.SerialException:
                pass
        self.serial = None
        self.connected = False

    def open_serial(self):
        with self.serial_lock:
            if self.serial is not None and self.serial.is_open:
                return True
            try:
                self.serial = serial.Serial(self.port, self.baud, timeout=0.05, write_timeout=0.2)
                self.connected = True
                self.queue.put(("log", f"connected {self.port}"))
                self.send_locked(f"speaker viz on {self.decimation}")
                self.send_locked("speaker params")
                self.send_locked("speaker status")
                return True
            except (serial.SerialException, OSError) as exc:
                self.close_serial_locked()
                self.queue.put(("log", f"waiting for {self.port}: {exc}"))
                return False

    def schedule_param(self, command, value):
        if command in self.pending:
            self.root.after_cancel(self.pending[command])
        self.pending[command] = self.root.after(
            90, lambda cmd=command, val=float(value): self.send_param(cmd, val)
        )

    def send_param(self, command, value):
        self.pending.pop(command, None)
        self.send(f"speaker {command} {value:.4f}")

    def read_loop(self):
        while self.running:
            if not self.open_serial():
                time.sleep(1.0)
                continue
            try:
                with self.serial_lock:
                    port = self.serial
                if port is None:
                    time.sleep(0.2)
                    continue
                raw = port.readline()
            except (serial.SerialException, OSError) as exc:
                self.queue.put(("log", f"serial read failed; reconnecting: {exc}"))
                with self.serial_lock:
                    self.close_serial_locked()
                time.sleep(0.5)
                continue
            if not raw:
                continue
            line = raw.decode("ascii", errors="ignore").strip()
            if line.startswith("$OUT,"):
                parts = line.split(",")
                if len(parts) >= 10:
                    try:
                        primary_hp = int(parts[3])
                        reference_hp = int(parts[4])
                        primary_raw = int(parts[10]) if len(parts) >= 12 else primary_hp
                        reference_raw = int(parts[11]) if len(parts) >= 12 else reference_hp
                        self.queue.put(
                            (
                                "out",
                                int(parts[1]),
                                int(parts[2]),
                                primary_hp,
                                reference_hp,
                                int(parts[5]),
                                int(parts[6]),
                                int(parts[7]),
                                int(parts[8]),
                                int(parts[9]),
                                primary_raw,
                                reference_raw,
                            )
                        )
                    except ValueError:
                        pass
            else:
                self.queue.put(("log", line))

    def tick(self):
        while True:
            try:
                item = self.queue.get_nowait()
            except queue.Empty:
                break
            if item[0] == "out":
                _, seq, out, primary_hp, reference_hp, p_env, r_env, out_env, updates, bins, primary_raw, reference_raw = item
                self.samples.append(out)
                self.primary_hp.append(primary_hp)
                self.reference_hp.append(reference_hp)
                self.primary_raw.append(primary_raw)
                self.reference_raw.append(reference_raw)
                self.last_sample_time = time.time()
                self.last_stats.update(
                    seq=seq,
                    p_env=p_env,
                    r_env=r_env,
                    out_env=out_env,
                    updates=updates,
                    bins=bins,
                )
            elif item[0] == "log":
                self.last_line = item[1]
                self.log_line(item[1])

        self.draw()
        self.root.after(25, self.tick)

    def draw_series(self, canvas, values, color, scale, width=1.5, zoom=1.0):
        if len(values) < 2:
            return
        canvas_width = int(canvas.winfo_width())
        canvas_height = int(canvas.winfo_height())
        top = 26
        bottom = canvas_height - 16
        mid = (top + bottom) / 2
        lane_height = bottom - top
        step = canvas_width / max(1, len(values) - 1)
        points = []
        for idx, sample in enumerate(values):
            x = idx * step
            y = mid - ((sample * zoom) / scale) * (lane_height * 0.42)
            points.extend((x, y))
        canvas.create_line(points, fill=color, width=width, smooth=False)

    def draw_meter(self, canvas, x, y, label, value, color):
        width = 190
        height = 14
        fill = min(width, int(width * min(1.0, value / 3000.0)))
        canvas.create_text(x, y - 10, text=f"{label} {value}", fill="#d8dee9", anchor="w", font=("Segoe UI", 9))
        canvas.create_rectangle(x, y, x + width, y + height, outline="#3b4252")
        canvas.create_rectangle(x, y, x + fill, y + height, outline="", fill=color)

    def draw_canvas_base(self, canvas, title, subtitle):
        canvas.delete("all")
        width = int(canvas.winfo_width())
        height = int(canvas.winfo_height())
        mid = (26 + height - 16) // 2
        canvas.create_line(0, mid, width, mid, fill="#2e3440")
        canvas.create_text(12, 16, text=title, fill="#f8fafc", anchor="w", font=("Segoe UI", 10, "bold"))
        canvas.create_text(150, 16, text=subtitle, fill="#cbd5e1", anchor="w", font=("Segoe UI", 9))

    def draw(self):
        raw_zoom = float(self.raw_zoom.get())
        output_zoom = float(self.output_zoom.get())
        self.draw_canvas_base(self.mic1_canvas, "Mic1 raw", f"raw blue, high-pass dark blue, zoom={raw_zoom:.1f}x")
        self.draw_canvas_base(self.mic2_canvas, "Mic2 raw", f"raw orange, high-pass dark orange, zoom={raw_zoom:.1f}x")
        self.draw_canvas_base(self.output_canvas, "GP10 output", f"final output before PWM, zoom={output_zoom:.1f}x")

        self.draw_series(self.mic1_canvas, self.primary_raw, "#60a5fa", 32768.0, width=1.8, zoom=raw_zoom)
        self.draw_series(self.mic1_canvas, self.primary_hp, "#1d4ed8", 12000.0, width=1.1, zoom=raw_zoom)
        self.draw_series(self.mic2_canvas, self.reference_raw, "#fb923c", 32768.0, width=1.8, zoom=raw_zoom)
        self.draw_series(self.mic2_canvas, self.reference_hp, "#9a3412", 12000.0, width=1.1, zoom=raw_zoom)
        self.draw_series(self.output_canvas, self.samples, "#22c55e", 12000.0, width=2.0, zoom=output_zoom)

        mic1_h = int(self.mic1_canvas.winfo_height())
        mic2_h = int(self.mic2_canvas.winfo_height())
        out_h = int(self.output_canvas.winfo_height())
        self.draw_meter(self.mic1_canvas, 12, mic1_h - 24, "mic1_env", self.last_stats["p_env"], "#60a5fa")
        self.draw_meter(self.mic2_canvas, 12, mic2_h - 24, "mic2_env", self.last_stats["r_env"], "#fb923c")
        self.draw_meter(self.output_canvas, 12, out_h - 24, "out_env", self.last_stats["out_env"], "#22c55e")
        stale = time.time() - self.last_sample_time
        self.status.set(
            f"port={self.port} {'connected' if self.connected else 'reconnecting'} seq={self.last_stats['seq']} "
            f"updates={self.last_stats['updates']} bins={self.last_stats['bins']} "
            f"last_sample={stale:.1f}s ago"
        )

    def close(self):
        self.running = False
        self.send("speaker viz off")
        time.sleep(0.05)
        with self.serial_lock:
            self.close_serial_locked()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description="Live visualizer and slider tuner for Plan A_7_6.")
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--decim", type=int, default=64)
    args = parser.parse_args()

    root = tk.Tk()
    LiveTuner(root, args.port, args.baud, args.decim)
    root.mainloop()


if __name__ == "__main__":
    main()
