#!/usr/bin/env python3
"""R5 leak matrix (Concealment story con-034) capture analysis. Emits observations, never a verdict.

Reads the captures and the log of `Stealth.R5Matrix` (Saved/Harness/R5Matrix: <Cell>_<State>_<n>.png, the single-frame
sequences <Prefix>_Enter_fNNN.png / <Prefix>_Exit_fNNN.png, Lod<k>_Hidden_<n>.png, Magenta<dd>m_*.png, r5matrix.jsonl).

Per lighting cell (as tools/analysis/r5_diff.py: luma-weighted RGB distance, 0-255):
  - noise floor (p99 |EmptyA - EmptyB|), the subject ROI (|Visible - EmptyA| > 4x floor), inside it the mean |Hidden -
    EmptyA| against the mean |EmptyB - EmptyA|, and the whole-frame counts above the floor;
  - the single frames around each swap: for every enter frame at or after the swap and every exit frame before the leaf
    crossing (the hidden window, from the log's pose start and server time), the ROI mean |frame - EmptyA mean|, against
    the single-frame reference: the largest ROI mean |EmptyB_n - EmptyA mean| over the six EmptyB frames.
Per LOD: every primitive's render proxy in the Hidden state (from the log) and the ROI residual against Lod_Empty.
Magenta rows (5/15/25 m): magenta pixels per frame (R and B high, G low); the hidden window's frames and their total, and
the control (frames before the swap, and the Visible captures) that shows the detector sees the body.

Usage: python tools/analysis/r5_matrix.py [captures_dir]
"""
import json
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from r5_diff import distance  # noqa: E402

CELLS = ("LowSun_VSM1", "LowSun_VSM0", "Noon_VSM1", "Noon_VSM0", "Shade_VSM1", "Shade_VSM0")
SWAP_SEC = 0.5
CROSS_SEC = 0.45
POSE_BUSH_ENTER = 5
POSE_BUSH_EXIT = 6


def load(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def mean_of(directory, prefix, count=6):
    frames = [load(os.path.join(directory, f"{prefix}_{n}.png")) for n in range(count) if os.path.exists(os.path.join(directory, f"{prefix}_{n}.png"))]
    return (np.mean(frames, axis=0), frames) if frames else (None, [])


def read_log(directory):
    shots = {}
    states = {}
    poses = {}
    path = os.path.join(directory, "r5matrix.jsonl")
    if not os.path.exists(path):
        return shots, states, poses
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("event") == "shot":
                shots[row["file"]] = row
            elif row.get("event") == "state":
                states[row["name"]] = row
    return shots, states, poses


def in_hidden_window(shot, kind):
    """True when the frame's server time lies in [swap, crossing) of its transition (from its pose and start). The time is
    the shot's effective time: a screenshot request is served by the NEXT rendered frame, so a sequence's image k shows the
    frame logged as shot k + 1 (set by effective_times)."""
    now, start, pose = shot.get("effective", shot["now"]), shot["start"], shot["pose"]
    if kind == "Enter":
        return pose == POSE_BUSH_ENTER and now >= start + SWAP_SEC
    if kind == "Exit":
        return pose != POSE_BUSH_EXIT or now < start + CROSS_SEC
    return False


def sequence(directory, prefix, shots=None):
    names = sorted(n for n in os.listdir(directory) if n.startswith(prefix + "_f") and n.endswith(".png"))
    if shots is not None:
        effective_times(names, shots)
    return names


def effective_times(names, shots):
    """Each image's effective time: the next logged frame's time (the last one adds the sequence's mean step)."""
    times = [shots[n]["now"] for n in names if n in shots]
    step = (times[-1] - times[0]) / (len(times) - 1) if len(times) > 1 else 0.0
    for index, name in enumerate(names):
        if name not in shots:
            continue
        shots[name]["effective"] = shots[names[index + 1]]["now"] if index + 1 < len(names) and names[index + 1] in shots else shots[name]["now"] + step


BIG = 10.0  # luma levels: a pixel that changed visibly


def magenta_pixels(image):
    """The magenta-tinted body: the appearance tint (1, 0, 1) leaves red and blue well above green on every drawn body pixel
    (pink to deep magenta by exposure). Counted above the HUD band only (the HUD's coloured text is excluded by the crop)."""
    body = image[:int(image.shape[0] * 0.78)]
    r, g, b = body[..., 0], body[..., 1], body[..., 2]
    return int(((r - g > 60) & (b - g > 60)).sum())


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else os.path.join("Saved", "Harness", "R5Matrix")
    shots, states, _ = read_log(directory)
    for cell in CELLS:
        empty_a, _ = mean_of(directory, f"{cell}_EmptyA")
        empty_b, empty_b_frames = mean_of(directory, f"{cell}_EmptyB")
        visible, _ = mean_of(directory, f"{cell}_Visible")
        hidden, _ = mean_of(directory, f"{cell}_Hidden")
        if any(m is None for m in (empty_a, empty_b, visible, hidden)):
            print(json.dumps({"cell": cell, "error": "missing captures"}))
            continue
        noise = distance(empty_a, empty_b)
        floor = float(np.percentile(noise, 99))
        roi = distance(visible, empty_a) > 4.0 * floor
        hidden_d = distance(hidden, empty_a)
        single_ref = max(int((distance(f, empty_a) > BIG).sum()) for f in empty_b_frames)
        row = {
            "cell": cell,
            "roiPixels": int(roi.sum()),
            "noiseFloorP99": round(floor, 3),
            "roiHiddenMean": round(float(hidden_d[roi].mean()), 3) if roi.any() else None,
            "roiNoiseMean": round(float(noise[roi].mean()), 3) if roi.any() else None,
            "hiddenPixelsAboveFloor": int((hidden_d > floor).sum()),
            "noisePixelsAboveFloor": int((noise > floor).sum()),
            "singleFrameReferencePixelsAbove10": single_ref,
            "hiddenProxies": [p["name"] for p in states.get(f"{cell}_Hidden", {}).get("primitives", []) if p.get("proxy")],
        }
        for kind in ("Enter", "Exit"):
            frames = sequence(directory, f"{cell}_{kind}", shots)
            window = [n for n in frames if n in shots and in_hidden_window(shots[n], kind)]
            worst = None
            above = []
            for name in window:
                d = int((distance(load(os.path.join(directory, name)), empty_a) > BIG).sum())
                worst = d if worst is None else max(worst, d)
                if d > 2 * single_ref + 50:
                    above.append({"frame": name, "pixelsAbove10": d, "effective": round(shots[name]["effective"], 4), "start": shots[name]["start"]})
            row[f"{kind.lower()}Frames"] = len(frames)
            row[f"{kind.lower()}HiddenWindowFrames"] = len(window)
            row[f"{kind.lower()}HiddenWindowWorstPixelsAbove10"] = worst
            row[f"{kind.lower()}FramesAboveSingleFrameReference"] = above
        print(json.dumps(row))

    lod_empty, _ = mean_of(directory, "Lod_Empty", 3)
    for k in range(16):
        lod_hidden, _ = mean_of(directory, f"Lod{k}_Hidden", 3)
        if lod_hidden is None:
            break
        state = states.get(f"Lod{k}_Hidden", {})
        print(json.dumps({
            "lod": k,
            "proxies": [p["name"] for p in state.get("primitives", []) if p.get("proxy")],
            "visibleFlags": [p["name"] for p in state.get("primitives", []) if p.get("visible")],
            "residualP99VsEmpty": round(float(np.percentile(distance(lod_hidden, lod_empty), 99)), 3) if lod_empty is not None else None,
        }))

    for metres in (5, 15, 25):
        prefix = f"Magenta{metres:02d}m"
        out = {"magenta": prefix}
        visible_px = [magenta_pixels(load(os.path.join(directory, f"{prefix}_Visible_{n}.png"))) for n in range(2) if os.path.exists(os.path.join(directory, f"{prefix}_Visible_{n}.png"))]
        out["visibleControlPixels"] = visible_px
        for kind in ("Enter", "Exit"):
            frames = sequence(directory, f"{prefix}_{kind}", shots)
            window, before, offenders = 0, [], []
            total = 0
            for name in frames:
                if name not in shots:
                    continue
                px = magenta_pixels(load(os.path.join(directory, name)))
                if in_hidden_window(shots[name], kind):
                    window += 1
                    total += px
                    if px > 0:
                        offenders.append({"frame": name, "pixels": px, "now": shots[name]["now"], "start": shots[name]["start"]})
                elif kind == "Enter":
                    before.append(px)
            out[f"{kind.lower()}HiddenWindowFrames"] = window
            out[f"{kind.lower()}HiddenWindowMagentaPixels"] = total
            out[f"{kind.lower()}Offenders"] = offenders
            if kind == "Enter":
                out["enterBeforeSwapMaxPixels"] = max(before) if before else None
        hidden_px = [magenta_pixels(load(os.path.join(directory, f"{prefix}_Hidden_{n}.png"))) for n in range(3) if os.path.exists(os.path.join(directory, f"{prefix}_Hidden_{n}.png"))]
        out["hiddenCapturePixels"] = hidden_px
        print(json.dumps(out))


if __name__ == "__main__":
    main()
