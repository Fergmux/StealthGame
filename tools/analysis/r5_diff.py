#!/usr/bin/env python3
"""Spike R5 (Concealment story con-021) capture analysis. Emits observations, never a verdict.

Reads the captures of `Stealth.Spike.R5` (Saved/Harness/R5/<Cell>_<State>_<n>.png, six per state) and, per matrix cell:
  - the temporal mean of each state (EmptyA, Visible, Hidden, EmptyB) in linear 0-255 luma-weighted RGB distance;
  - the noise floor: the 99th percentile of |EmptyA - EmptyB| over every pixel;
  - the hidden residual: the 99th percentile of |Hidden - EmptyA| and the count of pixels above the noise floor;
  - the visible control: the 99th percentile of |Visible - EmptyA| and the count above the floor (must be large: the
    subject and its shadow are in frame);
  - the subject region (ROI): the pixels where the visible control exceeds 4x the floor (the body, its shadow and any
    lighting it changes), and inside it the mean |Hidden - EmptyA| against the mean |EmptyB - EmptyA| (a leak raises the
    first above the second).
Writes a diff heat image per cell (Hidden vs EmptyA, amplified x8) next to the captures.

Usage: python tools/analysis/r5_diff.py [captures_dir]
"""
import json
import os
import sys

import numpy as np
from PIL import Image

STATES = ("EmptyA", "Visible", "Hidden", "EmptyB")


def mean_of(directory, cell, state):
    frames = []
    for n in range(6):
        path = os.path.join(directory, f"{cell}_{state}_{n}.png")
        if os.path.exists(path):
            frames.append(np.asarray(Image.open(path).convert("RGB"), dtype=np.float64))
    if not frames:
        return None
    return np.mean(frames, axis=0)


def distance(a, b):
    # Per-pixel colour distance, luma weighted (Rec. 709) so a shadow's darkening counts in proportion to what a viewer sees.
    d = np.abs(a - b)
    return d[..., 0] * 0.2126 + d[..., 1] * 0.7152 + d[..., 2] * 0.0722


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else os.path.join("Saved", "Harness", "R5")
    cells = sorted({name.rsplit("_", 2)[0] for name in os.listdir(directory) if name.endswith(".png") and "_diff" not in name})
    rows = []
    for cell in cells:
        means = {state: mean_of(directory, cell, state) for state in STATES}
        if any(m is None for m in means.values()):
            rows.append({"cell": cell, "error": "missing captures"})
            continue
        noise = distance(means["EmptyA"], means["EmptyB"])
        floor = float(np.percentile(noise, 99))
        hidden = distance(means["Hidden"], means["EmptyA"])
        visible = distance(means["Visible"], means["EmptyA"])
        heat = np.clip(hidden * 8.0, 0, 255).astype(np.uint8)
        Image.fromarray(heat).save(os.path.join(directory, f"{cell}_diff_hidden_x8.png"))
        roi = visible > 4.0 * floor
        roi_n = int(roi.sum())
        rows.append({
            "cell": cell,
            "roiPixels": roi_n,
            "roiHiddenMean": round(float(hidden[roi].mean()), 3) if roi_n else None,
            "roiNoiseMean": round(float(noise[roi].mean()), 3) if roi_n else None,
            "roiVisibleMean": round(float(visible[roi].mean()), 3) if roi_n else None,
            "pixels": int(noise.size),
            "noiseFloorP99": round(floor, 3),
            "noiseMax": round(float(noise.max()), 3),
            "hiddenP99": round(float(np.percentile(hidden, 99)), 3),
            "hiddenMax": round(float(hidden.max()), 3),
            "hiddenPixelsAboveFloor": int((hidden > floor).sum()),
            "noisePixelsAboveFloor": int((noise > floor).sum()),
            "visibleP99": round(float(np.percentile(visible, 99)), 3),
            "visiblePixelsAboveFloor": int((visible > floor).sum()),
        })
    for row in rows:
        print(json.dumps(row))


if __name__ == "__main__":
    main()
