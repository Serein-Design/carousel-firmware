#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""
Converts an animated GIF, a PNG or a folder of images into an animation for
the 64x32 HUB75 panel.

Produces two things:
  - a <script data-example="NAME"> block to paste into simulator.html
  - a .h file to include in firmware/src/screens/

The images are downscaled with nearest neighbour (no blur, the pixel art
stays crisp), mapped onto a palette shared by all the frames, then
RLE-compressed. On pixel art, RLE cuts the size by a factor of 5 to 10.

Examples
--------
  python pixelart.py cat.gif --name CATANIM
  python pixelart.py sprite.png --name ROCKET --colors 8 --fit cover
  python pixelart.py frames/ --name WALK --speed 3

Licences: only use images you have the right to use.
CC0 sources: kenney.nl, opengameart.org (CC0 filter), itch.io (CC0 tag).
"""

import argparse
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow missing:  python -m pip install Pillow")


# ---------------------------------------------------------------------------
# Loading the images
# ---------------------------------------------------------------------------

def load_frames(path: Path) -> list:
    """Returns the list of RGBA images, whether given a GIF, a PNG or a folder."""
    if path.is_dir():
        files = sorted(
            p for p in path.iterdir()
            if p.suffix.lower() in (".png", ".gif", ".jpg", ".jpeg", ".bmp", ".webp")
        )
        if not files:
            sys.exit(f"No image in {path}")
        return [Image.open(p).convert("RGBA") for p in files]

    img = Image.open(path)
    frames = []
    try:
        while True:
            frames.append(img.convert("RGBA").copy())
            img.seek(img.tell() + 1)
    except EOFError:
        pass
    return frames


def slice_sheet(img: Image.Image, cols: int, rows: int) -> list:
    """Slices a sprite sheet into frames, left to right then top to bottom.
    That is the layout of nearly every sprite sheet."""
    cw, ch = img.width // cols, img.height // rows
    if cw == 0 or ch == 0:
        sys.exit(f"Grid {cols}x{rows} too fine for a {img.width}x{img.height} image")
    return [img.crop((c * cw, r * ch, (c + 1) * cw, (r + 1) * ch))
            for r in range(rows) for c in range(cols)]


def flatten(img: Image.Image, bg) -> Image.Image:
    """Composites the transparency onto the background colour: the LED panel
    has no alpha channel, a transparent pixel must become an unlit pixel."""
    if img.mode != "RGBA":
        return img.convert("RGB")
    base = Image.new("RGBA", img.size, bg + (255,))
    return Image.alpha_composite(base, img).convert("RGB")


def fit(img: Image.Image, width: int, height: int, mode: str, bg) -> Image.Image:
    """Brings the image to the panel's size, with nearest neighbour."""
    if mode == "stretch":
        return img.resize((width, height), Image.NEAREST)

    ratio_src = img.width / img.height
    ratio_dst = width / height

    if (mode == "contain") == (ratio_src > ratio_dst):
        nw, nh = width, max(1, round(width / ratio_src))
    else:
        nh, nw = height, max(1, round(height * ratio_src))

    resized = img.resize((nw, nh), Image.NEAREST)

    if mode == "cover":                       # crop the overflow
        left = (nw - width) // 2
        top = (nh - height) // 2
        return resized.crop((left, top, left + width, top + height))

    canvas = Image.new("RGB", (width, height), bg)   # contain: background bars
    canvas.paste(resized, ((width - nw) // 2, (height - nh) // 2))
    return canvas


# ---------------------------------------------------------------------------
# Palette and compression
# ---------------------------------------------------------------------------

def prepare(frames: list, n_colors: int, sprite: bool, bg):
    """Returns (palette, indices per frame).

    In sprite mode index 0 is reserved for transparency: the decoder skips
    those runs instead of drawing them, which lets the sprite be laid over a
    background. In panel mode, the transparency is flattened onto the
    background colour."""
    if not sprite:
        opaque = [flatten(fr, bg) for fr in frames]
        ref, pal = shared_palette(opaque, n_colors)
        return pal, index_frames(opaque, ref)

    masks = [fr.getchannel("A").point(lambda a: 255 if a >= 128 else 0)
             for fr in frames]
    opaque = [flatten(fr, (0, 0, 0)) for fr in frames]
    ref, pal = shared_palette(opaque, max(2, n_colors - 1))
    raw = index_frames(opaque, ref)

    out = []
    for idx, mask in zip(raw, masks):
        mb = mask.tobytes()
        out.append([0 if mb[i] == 0 else idx[i] + 1 for i in range(len(idx))])
    return [(0, 0, 0)] + pal, out


def shared_palette(frames: list, n_colors: int):
    """A single palette for the whole animation, otherwise the colours jump."""
    width, height = frames[0].size
    stack = Image.new("RGB", (width, height * len(frames)))
    for i, fr in enumerate(frames):
        stack.paste(fr, (0, i * height))

    reduced = stack.quantize(colors=n_colors, method=Image.Quantize.MEDIANCUT)

    # getpalette() can return fewer entries than requested if the image
    # contains few distinct colours: only keep what actually exists.
    raw = reduced.getpalette() or []
    available = min(n_colors, len(raw) // 3)
    return reduced, [tuple(raw[i * 3:i * 3 + 3]) for i in range(available)]


def rgb565(c) -> int:
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def index_frames(frames: list, ref: Image.Image) -> list:
    """Maps each frame onto the reference palette.

    A mode P image already stores one index per byte: tobytes() gives the
    index array directly, without going through getdata()."""
    return [list(fr.quantize(palette=ref, dither=Image.Dither.NONE).tobytes())
            for fr in frames]


def compress_rle(indices: list, width: int) -> list:
    """(length, index) pairs. Runs are cut at the end of each row so that
    the decoder can draw each run with a single drawFastHLine."""
    stream = []
    for start in range(0, len(indices), width):
        row = indices[start:start + width]
        i = 0
        while i < len(row):
            val = row[i]
            n = 1
            while i + n < len(row) and row[i + n] == val and n < 255:
                n += 1
            stream.extend((n, val))
            i += n
    return stream


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

# The simulator relies on these generated names:
#   - data-example="..."   the attribute simulator.html scans for animations
#                          (querySelectorAll('script[data-example]'))
#   - data-orientation     'landscape', 'portrait' or 'both' (both galleries)
#   - anim(f, init)        the entry point the simulator calls every frame
ORIENTATION_BOTH = "both"           # fits both the landscape and portrait galleries
ORIENTATION_LANDSCAPE = "landscape"
ORIENTATION_PORTRAIT = "portrait"


def block(values, per_line, indent="  "):
    lines = []
    for i in range(0, len(values), per_line):
        lines.append(indent + ", ".join(str(v) for v in values[i:i + per_line]) + ",")
    return "\n".join(lines).rstrip(",")


def js_sprite_output(name, width, height, speed, palette, offsets, stream):
    """Moving sprite: a blit function + a demo animation that walks the
    character across from left to right."""
    pal = ", ".join(f"[{c[0]},{c[1]},{c[2]}]" for c in palette)
    return f'''<script type="text/plain" data-example="{name}" data-orientation="{ORIENTATION_BOTH}">
// Generated by tools/pixelart/pixelart.py - sprite {width}x{height}, {len(offsets) - 1} frames,
// {len(palette)} colours. Index 0 is transparent.
const PAL_RGB = [{pal}];
const SPR_W = {width}, SPR_H = {height}, SPEED = {speed};
const OFFS = [{", ".join(str(o) for o in offsets)}];
const RLE = [
{block(stream, 24)}
];

let PAL = null;

// Blits one frame of the sprite at (ox, oy). Runs of index 0 are skipped.
function blit(ox, oy, img) {{
  let p = OFFS[img];
  const end = OFFS[img + 1];
  let x = 0, y = 0;
  while (p < end) {{
    const n = RLE[p], idx = RLE[p + 1];
    p += 2;
    if (idx !== 0) display.drawFastHLine(ox + x, oy + y, n, PAL[idx]);
    x += n;
    if (x >= SPR_W) {{ x = 0; y++; }}
  }}
}}

function anim(f, init) {{
  if (init || !PAL) PAL = PAL_RGB.map(c => color565(c[0], c[1], c[2]));

  display.clearScreen();

  // Scenery: gradient sky and ground
  for (let y = 0; y < H(); y++) {{
    const k = 1 - y / H();
    display.drawFastHLine(0, y, W(), color565(int(8 + 22 * k), int(10 + 26 * k), int(20 + 40 * k)));
  }}
  display.drawFastHLine(0, H() - 2, W(), color565(38, 52, 34));
  display.drawFastHLine(0, H() - 1, W(), color565(24, 34, 22));

  // The character walks across in a loop
  const img = int(f / SPEED) % (OFFS.length - 1);
  const track = W() + SPR_W;
  const x = ((f * 0.55) % track) - SPR_W;
  blit(int(x), H() - SPR_H - 2, img);
}}
</script>'''


def cpp_sprite_output(name, width, height, speed, palette, offsets, stream):
    lower = name.lower()
    pal565 = [rgb565(c) for c in palette]
    return f'''// Generated by tools/pixelart/pixelart.py - do not edit by hand.
// Sprite {width}x{height}, {len(offsets) - 1} frames, {len(palette)} colours.
// Index 0 is transparent: those runs are not drawn.
#pragma once
#include <Arduino.h>

#define {name}_WIDTH   {width}
#define {name}_HEIGHT  {height}
#define {name}_FRAMES  {len(offsets) - 1}
#define {name}_SPEED   {speed}

const uint16_t {lower}_palette[] PROGMEM = {{
{block(pal565, 8)}
}};

const uint16_t {lower}_offsets[] PROGMEM = {{
{block(offsets, 12)}
}};

const uint8_t {lower}_rle[] PROGMEM = {{
{block(stream, 20)}
}};

// Blits one frame of the sprite at (ox, oy), honouring transparency.
inline void blit_{lower}(int ox, int oy, uint16_t img) {{
  uint16_t p   = pgm_read_word(&{lower}_offsets[img]);
  uint16_t end = pgm_read_word(&{lower}_offsets[img + 1]);
  int x = 0, y = 0;
  while (p < end) {{
    uint8_t n   = pgm_read_byte(&{lower}_rle[p]);
    uint8_t idx = pgm_read_byte(&{lower}_rle[p + 1]);
    p += 2;
    if (idx != 0)
      display->drawFastHLine(ox + x, oy + y, n,
                             pgm_read_word(&{lower}_palette[idx]));
    x += n;
    if (x >= {name}_WIDTH) {{ x = 0; y++; }}
  }}
}}

// Current frame from the display frame counter
inline uint16_t frame_{lower}(uint32_t f) {{
  return (f / {name}_SPEED) % {name}_FRAMES;
}}'''


def js_output(name, width, height, speed, palette, offsets, stream, orientation):
    pal = ", ".join(f"[{c[0]},{c[1]},{c[2]}]" for c in palette)
    return f'''<script type="text/plain" data-example="{name}" data-orientation="{orientation}">
// Generated by tools/pixelart/pixelart.py - {len(offsets) - 1} frames {width}x{height},
// {len(palette)} colours, {len(stream) // 2} RLE runs.
const PAL_RGB = [{pal}];
const WIDTH = {width}, HEIGHT = {height}, SPEED = {speed};
const OFFS = [{", ".join(str(o) for o in offsets)}];
const RLE = [
{block(stream, 24)}
];

let PAL = null;

function anim(f, init) {{
  if (init || !PAL) PAL = PAL_RGB.map(c => color565(c[0], c[1], c[2]));

  const img = int(f / SPEED) % (OFFS.length - 1);
  const ox = int((W() - WIDTH) / 2), oy = int((H() - HEIGHT) / 2);

  display.clearScreen();

  let p = OFFS[img];
  const end = OFFS[img + 1];
  let x = 0, y = 0;
  while (p < end) {{
    const n = RLE[p], idx = RLE[p + 1];
    p += 2;
    display.drawFastHLine(ox + x, oy + y, n, PAL[idx]);
    x += n;
    if (x >= WIDTH) {{ x = 0; y++; }}
  }}
}}
</script>'''


def cpp_output(name, width, height, speed, palette, offsets, stream):
    lower = name.lower()
    pal565 = [rgb565(c) for c in palette]
    return f'''// Generated by tools/pixelart/pixelart.py - do not edit by hand.
// {len(offsets) - 1} frames {width}x{height}, {len(palette)} colours,
// {len(stream) // 2} RLE runs, {len(stream) + len(pal565) * 2 + len(offsets) * 2} bytes of data.
#pragma once
#include <Arduino.h>

#define {name}_WIDTH   {width}
#define {name}_HEIGHT  {height}
#define {name}_FRAMES  {len(offsets) - 1}
#define {name}_SPEED   {speed}   // display frames per animation frame

const uint16_t {lower}_palette[] PROGMEM = {{
{block(pal565, 8)}
}};

const uint16_t {lower}_offsets[] PROGMEM = {{
{block(offsets, 12)}
}};

const uint8_t {lower}_rle[] PROGMEM = {{
{block(stream, 20)}
}};

// Call from an anim(f, init) function: the RLE runs are already cut at the
// end of each row, each one becomes a drawFastHLine.
inline void draw_{lower}(uint32_t f) {{
  const uint16_t img = (f / {name}_SPEED) % {name}_FRAMES;
  const int ox = (display->width()  - {name}_WIDTH) / 2;
  const int oy = (display->height() - {name}_HEIGHT) / 2;

  uint16_t p   = pgm_read_word(&{lower}_offsets[img]);
  uint16_t end = pgm_read_word(&{lower}_offsets[img + 1]);
  int x = 0, y = 0;

  display->clearScreen();
  while (p < end) {{
    uint8_t n   = pgm_read_byte(&{lower}_rle[p]);
    uint8_t idx = pgm_read_byte(&{lower}_rle[p + 1]);
    p += 2;
    display->drawFastHLine(ox + x, oy + y, n,
                           pgm_read_word(&{lower}_palette[idx]));
    x += n;
    if (x >= {name}_WIDTH) {{ x = 0; y++; }}
  }}
}}'''


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description="GIF / PNG / folder -> HUB75 64x32 animation",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", type=Path, help="animated GIF, image, or folder of frames")
    ap.add_argument("--name", required=True, help="Identifier (UPPERCASE, no spaces)")
    ap.add_argument("--width", type=int, default=64)
    ap.add_argument("--height", type=int, default=32)
    ap.add_argument("--colors", type=int, default=16, help="2 to 256 (default 16)")
    ap.add_argument("--speed", type=int, default=4,
                    help="display frames per animation frame (default 4)")
    ap.add_argument("--fit", choices=("contain", "cover", "stretch"),
                    default="contain",
                    help="contain = bars, cover = crop, stretch = distort")
    ap.add_argument("--background", default="0,0,0", help="colour of the bars, R,G,B")
    ap.add_argument("--grid", default="",
                    help="slices a sprite sheet, e.g. 4x1 (columns x rows)")
    ap.add_argument("--row", type=int, default=-1,
                    help="only keep one row of the grid (0 = the first)")
    ap.add_argument("--sprite", action="store_true",
                    help="keeps the native size and the transparency, for a "
                         "sprite moved over a background")
    ap.add_argument("--max-frames", type=int, default=0,
                    help="limits the number of frames (0 = all)")
    ap.add_argument("--out-dir", type=Path, default=Path("."),
                    help="where to write the generated files")
    args = ap.parse_args()

    name = args.name.upper().replace(" ", "_").replace("-", "_")
    if not args.source.exists():
        sys.exit(f"Not found: {args.source}")
    if not 2 <= args.colors <= 256:
        sys.exit("--colors must be between 2 and 256")

    bg = tuple(int(v) for v in args.background.split(","))

    frames = load_frames(args.source)
    print(f"{len(frames)} image(s) read, source {frames[0].width}x{frames[0].height}")

    if args.grid:
        try:
            cols, rows = (int(v) for v in args.grid.lower().split("x"))
        except ValueError:
            sys.exit("--grid expects the form COLUMNSxROWS, for example 4x1")
        sliced = []
        for fr in frames:
            grid = slice_sheet(fr, cols, rows)
            if args.row >= 0:
                if args.row >= rows:
                    sys.exit(f"--row {args.row} outside the grid ({rows} rows)")
                grid = grid[args.row * cols:(args.row + 1) * cols]
            sliced.extend(grid)
        frames = sliced
        print(f"  -> sheet sliced into {len(frames)} frames "
              f"of {frames[0].width}x{frames[0].height}"
              + (f" (row {args.row})" if args.row >= 0 else ""))

    if args.max_frames and len(frames) > args.max_frames:
        step = len(frames) / args.max_frames
        frames = [frames[int(i * step)] for i in range(args.max_frames)]
        print(f"  -> reduced to {len(frames)} frame(s)")

    if args.sprite:
        width, height = frames[0].width, frames[0].height
        palette, indices = prepare(frames, args.colors, True, bg)
        print(f"mode      : sprite {width}x{height}, index 0 transparent")
    else:
        flat = [flatten(fr, bg) for fr in frames]
        flat = [fit(fr, args.width, args.height, args.fit, bg)
                for fr in flat]
        width, height = args.width, args.height
        palette, indices = prepare(flat, args.colors, False, bg)

    stream, offsets = [], [0]
    for idx in indices:
        stream.extend(compress_rle(idx, width))
        offsets.append(len(stream))

    raw = width * height * len(frames)
    n_bytes = len(stream) + len(palette) * 2 + len(offsets) * 2
    print(f"palette   : {len(palette)} colours")
    print(f"RLE       : {len(stream) // 2} runs")
    print(f"size      : {n_bytes} bytes  (raw {raw}, gain x{raw / max(n_bytes, 1):.1f})")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    f_js = args.out_dir / f"{name.lower()}.simulator.html"
    f_h = args.out_dir / f"{name.lower()}.h"

    if args.sprite:
        js = js_sprite_output(name, width, height, args.speed, palette, offsets, stream)
        h = cpp_sprite_output(name, width, height, args.speed, palette, offsets, stream)
    else:
        orientation = ORIENTATION_PORTRAIT if height > width else ORIENTATION_LANDSCAPE
        js = js_output(name, width, height, args.speed, palette, offsets, stream, orientation)
        h = cpp_output(name, width, height, args.speed, palette, offsets, stream)

    f_js.write_text(js, encoding="utf-8")
    f_h.write_text(h, encoding="utf-8")

    print()
    print(f"wrote : {f_js}")
    print(f"        paste its contents into tools/simulator/simulator.html,")
    print(f"        then add '{name}' to GALLERY_ORDER.")
    print(f"wrote : {f_h}")
    print(f'        copy it into include/, then #include "{name.lower()}.h"')
    print(f"        and call draw_{name.lower()}(f) from an anim.")


if __name__ == "__main__":
    main()
