#!/usr/bin/env python3
"""Interactive AERA Recovery display mirror over USB/ADB."""
from __future__ import annotations

import argparse
import base64
import io
import json
import queue
import shutil
import subprocess
import sys
import threading
import time
import tkinter as tk

Image = None
ImageTk = None

INPUT_BRIDGE = r'''while IFS= read -r line; do
  printf '%s' "$line" > /system/bin/foxin &
  cat /system/bin/foxout >/dev/null
  wait
done'''


class Fox:
    def __init__(self, adb: str):
        self.adb = adb

    def rpc(self, op: str, args: dict, timeout: float = 8.0) -> dict:
        request = json.dumps({"v": 1, "id": "aera-mirror", "op": op,
                              "args": args}, separators=(",", ":")).encode()
        reader = subprocess.Popen(
            [self.adb, "exec-out", "cat", "/system/bin/foxout"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            writer = subprocess.run(
                [self.adb, "shell", "sh", "-c",
                 "cat > /system/bin/foxin"], input=request,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                timeout=timeout)
            if writer.returncode:
                raise RuntimeError(writer.stderr.decode(errors="replace").strip())
            output, error = reader.communicate(timeout=timeout)
        except Exception:
            reader.kill()
            reader.wait()
            raise
        if reader.returncode:
            raise RuntimeError(error.decode(errors="replace").strip())
        events = [json.loads(line) for line in output.splitlines() if line]
        result = next((item for item in reversed(events)
                       if item.get("event") == "result"), None)
        if not result or result.get("code") != 0:
            raise RuntimeError("AERA rejected the mirror command")
        return next((item.get("value", {}) for item in events
                     if item.get("event") == "data"), {})


class Mirror:
    def __init__(self, root: tk.Tk, adb: str, fps: int):
        self.root = root
        self.fox = Fox(adb)
        self.adb = adb
        self.fps = fps
        self.closed = threading.Event()
        self.frames: queue.Queue = queue.Queue(maxsize=1)
        self.input_events: queue.Queue[dict] = queue.Queue(maxsize=12)
        self.source_size = (1, 1)
        self.image_box = (0, 0, 1, 1)
        self.photo = None
        self.stream = None
        self.input = None
        self.last_move = 0.0

        root.title("AERA Mirror")
        root.geometry("480x900")
        root.minsize(280, 420)
        root.configure(bg="#111317")
        self.canvas = tk.Canvas(root, bg="#111317", highlightthickness=0,
                                cursor="crosshair")
        self.canvas.pack(fill="both", expand=True)
        self.status = tk.Label(root, text="Connecting to AERA…", anchor="w",
                               bg="#171a20", fg="#24c8ed", padx=12, pady=7)
        self.status.pack(fill="x")
        self.canvas.bind("<ButtonPress-1>", self.touch_down)
        self.canvas.bind("<B1-Motion>", self.touch_move)
        self.canvas.bind("<ButtonRelease-1>", self.touch_up)
        self.canvas.bind("<Button-3>", lambda _: self.send_key("back"))
        root.bind("<Escape>", lambda _: self.send_key("back"))
        root.bind("<BackSpace>", lambda _: self.send_key("back"))
        root.bind("<Home>", lambda _: self.send_key("home"))
        root.bind("<Configure>", lambda _: self.draw_latest())
        root.protocol("WM_DELETE_WINDOW", self.close)

    def start(self):
        try:
            self.fox.rpc("screenstream", {"action": "start", "fps": self.fps})
        except Exception as error:
            self.status.configure(text=f"Could not start mirror: {error}", fg="#ff6b77")
            return
        self.stream = subprocess.Popen(
            [self.adb, "exec-out", "cat", "/system/bin/foxscreenout"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1024 * 1024)
        self.input = subprocess.Popen(
            [self.adb, "shell", "sh", "-c", INPUT_BRIDGE],
            stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, text=True, bufsize=1)
        threading.Thread(target=self.read_frames, daemon=True).start()
        threading.Thread(target=self.write_input, daemon=True).start()
        self.status.configure(text=f"USB connected • {self.fps} FPS target • drag to touch")
        self.root.after(16, self.update)

    def read_frames(self):
        assert self.stream and self.stream.stdout
        while not self.closed.is_set():
            line = self.stream.stdout.readline()
            if not line:
                break
            try:
                frame = Image.open(io.BytesIO(base64.b64decode(line, validate=True)))
                frame.load()
                frame = frame.convert("RGB")
            except Exception:
                continue
            if self.frames.full():
                try:
                    self.frames.get_nowait()
                except queue.Empty:
                    pass
            self.frames.put_nowait(frame)

    def write_input(self):
        assert self.input and self.input.stdin
        while not self.closed.is_set():
            try:
                args = self.input_events.get(timeout=0.2)
            except queue.Empty:
                continue
            request = json.dumps({"v": 1, "id": "aera-input", "op": "input",
                                  "args": args}, separators=(",", ":"))
            try:
                self.input.stdin.write(request + "\n")
                self.input.stdin.flush()
            except (BrokenPipeError, OSError):
                break

    def update(self):
        if self.closed.is_set():
            return
        self.draw_latest()
        self.root.after(16, self.update)

    def draw_latest(self):
        frame = None
        try:
            while True:
                frame = self.frames.get_nowait()
        except queue.Empty:
            pass
        if frame is not None:
            self.current = frame
            self.source_size = frame.size
        elif not hasattr(self, "current"):
            return
        width = max(1, self.canvas.winfo_width())
        height = max(1, self.canvas.winfo_height())
        source_w, source_h = self.source_size
        scale = min(width / source_w, height / source_h)
        target = (max(1, round(source_w * scale)), max(1, round(source_h * scale)))
        x = (width - target[0]) // 2
        y = (height - target[1]) // 2
        self.image_box = (x, y, target[0], target[1])
        resized = self.current.resize(target, Image.Resampling.LANCZOS)
        self.photo = ImageTk.PhotoImage(resized)
        self.canvas.delete("frame")
        self.canvas.create_image(x, y, image=self.photo, anchor="nw", tags="frame")

    def point(self, event):
        x, y, width, height = self.image_box
        if event.x < x or event.y < y or event.x >= x + width or event.y >= y + height:
            return None
        source_w, source_h = self.source_size
        return (min(source_w - 1, max(0, (event.x - x) * source_w // width)),
                min(source_h - 1, max(0, (event.y - y) * source_h // height)))

    def enqueue(self, event: dict, important=False):
        try:
            self.input_events.put(event, timeout=0.15 if important else 0)
        except queue.Full:
            pass

    def touch_down(self, event):
        point = self.point(event)
        if point:
            self.enqueue({"action": "down", "x": point[0], "y": point[1]}, True)

    def touch_move(self, event):
        now = time.monotonic()
        if now - self.last_move < 1 / 45:
            return
        self.last_move = now
        point = self.point(event)
        if point:
            self.enqueue({"action": "move", "x": point[0], "y": point[1]})

    def touch_up(self, _event):
        self.enqueue({"action": "up"}, True)

    def send_key(self, key: str):
        self.enqueue({"action": "key", "key": key}, True)

    def close(self):
        if self.closed.is_set():
            return
        self.closed.set()
        for process in (self.stream, self.input):
            if process and process.poll() is None:
                process.terminate()
        try:
            self.fox.rpc("screenstream", {"action": "stop"}, timeout=3)
        except Exception:
            pass
        self.root.destroy()


def main():
    global Image, ImageTk
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", default=shutil.which("adb") or "adb")
    parser.add_argument("--fps", type=int, choices=range(1, 31), default=30)
    arguments = parser.parse_args()
    try:
        from PIL import Image as PillowImage, ImageTk as PillowImageTk
    except ImportError:
        raise SystemExit(
            "AERA Mirror requires Pillow: python3 -m pip install Pillow")
    Image, ImageTk = PillowImage, PillowImageTk
    if not shutil.which(arguments.adb) and not args_path(arguments.adb):
        raise SystemExit("adb was not found; pass its path using --adb")
    root = tk.Tk()
    mirror = Mirror(root, arguments.adb, arguments.fps)
    root.after(100, mirror.start)
    root.mainloop()


def args_path(value: str) -> bool:
    from pathlib import Path
    return Path(value).is_file()


if __name__ == "__main__":
    main()
