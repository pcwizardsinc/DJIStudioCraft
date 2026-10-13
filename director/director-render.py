#!/usr/bin/env python3
"""
director-render.py - render a 360 Director camera path to a flat video.

Reads the path file saved by 360-director.html, plays the full-resolution
equirectangular master through the recorded camera moves, and encodes a
normal (flat) video with the master's audio.

    cd ~/Videos/MyFlight            # the folder holding MyClip.mp4
    director-render ~/Downloads/MyClip.path.json

The finished video is written next to the master.

Needs: ffmpeg, ffprobe, python3-numpy, python3-opencv
    sudo dnf install -y python3-numpy python3-opencv
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import cv2
import numpy as np

# ---- camera path core (mirrors pathcore.js in 360-director.html exactly) ----
DEFAULT_POSE = {"yaw": 0.0, "pitch": 0.0, "roll": 0.0, "fov": 100.0}


def eval_path(keys, t):
    if not keys:
        return dict(DEFAULT_POSE)
    if t <= keys[0]["t"]:
        return pick(keys[0])
    last = keys[-1]
    if t >= last["t"]:
        return pick(last)
    lo, hi = 0, len(keys) - 1
    while hi - lo > 1:
        mid = (lo + hi) >> 1
        if keys[mid]["t"] <= t:
            lo = mid
        else:
            hi = mid
    a, b = keys[lo], keys[hi]
    span = b["t"] - a["t"]
    s = (t - a["t"]) / span if span > 0 else 1.0
    if b.get("ease", "smooth") != "linear":
        s = s * s * (3 - 2 * s)
    return {k: a[k] + (b[k] - a[k]) * s for k in ("yaw", "pitch", "roll", "fov")}


def pick(k):
    return {"yaw": k["yaw"], "pitch": k["pitch"], "roll": k["roll"], "fov": k["fov"]}
# ---- end camera path core ----


def die(msg):
    print(f"director-render: {msg}", file=sys.stderr)
    sys.exit(1)


def probe(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=width,height,r_frame_rate,color_space,color_range:format=duration",
         "-of", "json", path],
        capture_output=True, text=True)
    if out.returncode != 0:
        die(f"ffprobe could not read {path}:\n{out.stderr.strip()}")
    info = json.loads(out.stdout)
    st = info["streams"][0]
    num, den = (int(x) for x in st["r_frame_rate"].split("/"))
    return (st["width"], st["height"], num, den, float(info["format"]["duration"]),
            st.get("color_space", ""), st.get("color_range", ""))


class Projector:
    """Equirectangular -> rectilinear remap, same math as the viewer's shader."""

    def __init__(self, src_w, src_h, out_w, out_h, threads):
        self.W, self.H, self.w, self.h = src_w, src_h, out_w, out_h
        self.u = (2 * (np.arange(out_w, dtype=np.float32) + 0.5) / out_w - 1)[None, :]
        self.v = (1 - 2 * (np.arange(out_h, dtype=np.float32) + 0.5) / out_h)[:, None]
        self.mx = np.empty((out_h, out_w), np.float32)
        self.my = np.empty((out_h, out_w), np.float32)
        n = max(1, threads)
        step = math.ceil(out_h / n)
        self.bands = [(i, min(out_h, i + step)) for i in range(0, out_h, step)]
        self.pool = ThreadPoolExecutor(max_workers=n)
        self.last = None

    def _band(self, r0, r1, c):
        yw_c, yw_s, p_c, p_s, r_c, r_s, th, tv = c
        sx = self.u * th
        sy = self.v[r0:r1] * tv
        wx = sx * r_c + sy * r_s
        wy = -sx * r_s + sy * r_c
        n = 1.0 / np.sqrt(wx * wx + wy * wy + 1.0)
        x, y, z = wx * n, wy * n, n
        y2 = y * p_c + z * p_s
        z2 = -y * p_s + z * p_c
        x3 = x * yw_c + z2 * yw_s
        z3 = -x * yw_s + z2 * yw_c
        self.mx[r0:r1] = np.arctan2(x3, z3) * (self.W / (2 * math.pi)) + (self.W / 2 - 0.5)
        self.my[r0:r1] = (0.5 - np.arcsin(np.clip(y2, -1.0, 1.0)) / math.pi) * self.H - 0.5

    def maps(self, pose):
        key = tuple(round(pose[k], 6) for k in ("yaw", "pitch", "roll", "fov"))
        if key == self.last:
            return self.mx, self.my
        self.last = key
        yw, p, r = (math.radians(pose[k]) for k in ("yaw", "pitch", "roll"))
        hf = math.radians(min(max(pose["fov"], 1.0), 170.0))
        # fov is measured across the long side of the frame (matches the editor)
        t = math.tan(hf / 2)
        long_side = max(self.w, self.h)
        th = t * self.w / long_side
        tv = t * self.h / long_side
        c = tuple(np.float32(v) for v in (math.cos(yw), math.sin(yw), math.cos(p), math.sin(p),
                                          math.cos(r), math.sin(r), th, tv))
        list(self.pool.map(lambda b: self._band(b[0], b[1], c), self.bands))
        return self.mx, self.my


def main():
    ap = argparse.ArgumentParser(description="Render a 360 Director camera path to a flat video.")
    ap.add_argument("path", help="the .path.json file saved by 360 Director")
    ap.add_argument("--master", help="equirectangular master video (default: from the path file)")
    ap.add_argument("--out", help="output file (default: from the path file, saved next to the master)")
    ap.add_argument("--size", help="output WxH, e.g. 1920x1080 (default: from the path file)")
    ap.add_argument("--crf", type=int, help="quality, lower is better (default: from the path file)")
    ap.add_argument("--codec", choices=["libx265", "libx264"], help="video encoder")
    ap.add_argument("--preset", default="fast", help="encoder speed preset (default: fast)")
    ap.add_argument("--interp", choices=["linear", "cubic"], default="linear",
                    help="resampling filter (default: linear)")
    ap.add_argument("--threads", type=int, default=os.cpu_count() or 4,
                    help="threads for the projection maths (default: all cores)")
    a = ap.parse_args()

    for tool in ("ffmpeg", "ffprobe"):
        if not shutil.which(tool):
            die(f"{tool} not found. Install it with: sudo dnf install -y ffmpeg")

    try:
        with open(a.path) as f:
            proj = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        die(f"could not read {a.path}: {e}")
    base = os.path.dirname(os.path.abspath(a.path))

    name = os.path.expanduser(a.master or proj.get("master") or "")
    candidates = [name] if os.path.isabs(name) else [os.path.join(os.getcwd(), name), os.path.join(base, name)]
    master = next((c for c in candidates if os.path.isfile(c)), None)
    if not name or not master:
        die(f"can't find the 360 master '{name}'.\n"
            "Run this from the folder that holds it, or add --master /path/to/master.mp4")
    out_cfg = proj.get("output", {})
    if a.size:
        try:
            ow, oh = (int(x) for x in a.size.lower().split("x"))
        except ValueError:
            die("--size must look like 1920x1080")
    else:
        ow, oh = int(out_cfg.get("width", 1920)), int(out_cfg.get("height", 1080))
    if ow % 2 or oh % 2:
        die("output width and height must both be even numbers")
    crf = a.crf if a.crf is not None else int(out_cfg.get("crf", 20))
    codec = a.codec or out_cfg.get("codec", "libx265")
    out_name = os.path.expanduser(a.out or out_cfg.get("file") or os.path.basename(os.path.splitext(master)[0]) + ".director.mp4")
    out = out_name if os.path.isabs(out_name) else os.path.join(os.path.dirname(master), out_name)

    keys = sorted(proj.get("keys", []), key=lambda k: k["t"])
    src_w, src_h, fnum, fden, dur, colorspace, color_range = probe(master)
    if abs(src_w - 2 * src_h) > 2:
        print(f"warning: master is {src_w}x{src_h}, not 2:1; is it an equirectangular 360 render?",
              file=sys.stderr)
    fps = fnum / fden
    # Decode with the master's own colour matrix (OpenOSV tags bt709); untagged
    # files are treated as bt709 too, matching how the output is tagged.
    matrix = "bt709" if colorspace in ("", "unknown", "bt709") else "auto"
    rng_in = "full" if color_range == "pc" else "tv"
    rng = proj.get("range", {})
    t_in = max(0.0, float(rng.get("in") or 0.0))
    t_out = float(rng.get("out") or dur)
    t_out = min(t_out, dur)
    if t_out <= t_in:
        die(f"empty range: in {t_in:.3f}s, out {t_out:.3f}s")
    length = t_out - t_in
    total = int(round(length * fps))

    print(f"Master : {master} ({src_w}x{src_h} @ {fps:.3f} fps)")
    print(f"Output : {out} ({ow}x{oh}, {codec} crf {crf})")
    print(f"Range  : {t_in:.2f}s to {t_out:.2f}s, {total} frames, {len(keys)} keyframes")

    dec = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-ss", f"{t_in:.6f}", "-t", f"{length:.6f}", "-i", master,
         "-map", "0:v:0", "-vf", f"scale=in_color_matrix={matrix}:in_range={rng_in}",
         "-f", "rawvideo", "-pix_fmt", "bgr24", "-"],
        stdout=subprocess.PIPE, bufsize=src_w * src_h * 3)
    enc_cmd = ["ffmpeg", "-v", "error", "-y",
               "-f", "rawvideo", "-pix_fmt", "bgr24", "-s", f"{ow}x{oh}", "-r", f"{fnum}/{fden}", "-i", "-",
               "-ss", f"{t_in:.6f}", "-t", f"{length:.6f}", "-i", master,
               "-map", "0:v", "-map", "1:a?",
               "-vf", "scale=out_color_matrix=bt709:out_range=tv",
               "-c:v", codec, "-preset", a.preset, "-crf", str(crf),
               "-pix_fmt", "yuv420p", "-color_primaries", "bt709", "-color_trc", "bt709",
               "-colorspace", "bt709", "-c:a", "aac", "-b:a", "192k",
               "-movflags", "+faststart"]
    if codec == "libx265":
        enc_cmd += ["-tag:v", "hvc1"]
    enc = subprocess.Popen(enc_cmd + [out], stdin=subprocess.PIPE)

    projector = Projector(src_w, src_h, ow, oh, a.threads)
    interp = cv2.INTER_CUBIC if a.interp == "cubic" else cv2.INTER_LINEAR
    frame_bytes = src_w * src_h * 3
    n, t0, last_print = 0, time.time(), 0.0
    try:
        while True:
            buf = dec.stdout.read(frame_bytes)
            if len(buf) < frame_bytes:
                break
            src = np.frombuffer(buf, np.uint8).reshape(src_h, src_w, 3)
            pose = eval_path(keys, t_in + n / fps)
            mx, my = projector.maps(pose)
            frame = cv2.remap(src, mx, my, interp, borderMode=cv2.BORDER_WRAP)
            enc.stdin.write(frame.tobytes())
            n += 1
            now = time.time()
            if now - last_print >= 1.0 or n == total:
                rate = n / max(now - t0, 1e-6)
                eta = (total - n) / rate if rate > 0 else 0
                print(f"\r{n}/{total} frames  {rate:5.1f} fps  about {eta / 60:5.1f} min left ",
                      end="", flush=True)
                last_print = now
    except BrokenPipeError:
        die("the encoder stopped unexpectedly; see the ffmpeg message above")
    except KeyboardInterrupt:
        print("\nStopped. The partial file is not playable.")
        dec.kill(); enc.kill()
        sys.exit(130)
    finally:
        try:
            enc.stdin.close()
        except BrokenPipeError:
            pass
    dec.wait()
    code = enc.wait()
    print()
    if code != 0 or n == 0:
        die(f"encoding failed (ffmpeg exit {code}, {n} frames rendered)")
    print(f"Done: {out} ({n} frames in {(time.time() - t0) / 60:.1f} min)")


if __name__ == "__main__":
    main()
