"""Compose the D54 spike renders of one clip into a contact sheet (SCL story 056).

Run with the system Python (needs Pillow), after retarget_motion_to_mannequin.py has rendered the clip:
    python tools/asset-pipeline/motion_contact_sheet.py <renders dir> <clip> <out.png>
Rows: 3/4 close-up (review), greyscale profile at 20 m, profile at 30 m, front at 20 m. The 20/30 m crops are
shown at 2x nearest-neighbour so the true pixel count (1080p, 60-degree vertical FOV) stays visible.
"""
import glob
import os
import re
import sys

from PIL import Image, ImageDraw

ROWS = [("close", "3/4 close-up", None), ("profile20", "profile 20 m (2x)", 2), ("profile30", "profile 30 m (2x)", 2),
        ("front20", "front 20 m (2x)", 2)]


def main(rdir, clip, out):
    frames = sorted({int(m.group(1)) for p in glob.glob(os.path.join(rdir, f"{clip}_close_f*.png"))
                     for m in [re.search(r"_f(\d+)\.png$", p)] if m})
    cells = {}
    for key, _label, scale in ROWS:
        for f in frames:
            im = Image.open(os.path.join(rdir, f"{clip}_{key}_f{f:03d}.png")).convert("RGB")
            if scale:
                im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
            else:
                im.thumbnail((240, 240))
            cells[(key, f)] = im
    colw = max(im.width for im in cells.values()) + 8
    rowh = {key: max(cells[(key, f)].height for f in frames) + 8 for key, _l, _s in ROWS}
    lab = 130
    sheet = Image.new("RGB", (lab + colw * len(frames), 24 + sum(rowh.values())), "white")
    d = ImageDraw.Draw(sheet)
    d.text((4, 6), clip, fill="black")
    for j, f in enumerate(frames):
        d.text((lab + j * colw + 4, 6), f"f{f} ({(f - 1) / 30:.2f} s)", fill="black")
    y = 24
    for key, label, _s in ROWS:
        d.text((4, y + rowh[key] // 2), label, fill="black")
        for j, f in enumerate(frames):
            sheet.paste(cells[(key, f)], (lab + j * colw + 4, y + 4))
        y += rowh[key]
    sheet.save(out)
    print("SHEET", out, sheet.size)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
