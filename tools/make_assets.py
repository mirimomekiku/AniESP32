#!/usr/bin/env python3
"""Convert the AI-generated art in assets/v2/raw into palette-indexed C arrays for the CYD.

Reads  assets/v2/raw/*.png  (sprite sheets on magenta, plus full backgrounds)
Writes src/assets.h, src/assets.cpp and previews in assets/preview/

Sprites: sliced by connected component, scaled to a shared size, given an extra 1 px dark outline,
quantized to one shared palette per group (clean stepped bands, no dithering) and stored as
8-bit indices. Index 0 is transparent. Backgrounds are 240x320 palette-indexed.
"""
import os
import sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "v2", "raw")
PREVIEW = os.path.join(ROOT, "assets", "preview")
SRC = os.path.join(ROOT, "src")

KEY = 0xF81F
OUTLINE = (58, 34, 82)  # dark plum, matches the UI outline colour


def rgb565(r, g, b):
    # rounded, not truncated: truncating tints everything slightly green (green has more bits)
    return (((r * 31 + 127) // 255) << 11) | (((g * 63 + 127) // 255) << 5) | ((b * 31 + 127) // 255)


def key_dist2(px):
    r, g, b = px[:3]
    return (r - 255) ** 2 + g**2 + (b - 255) ** 2


def alpha_cut(img):
    """Remove the magenta background. Pixels very close to the key go everywhere; looser matches
    (anti-aliased edges) only when connected to the sheet border, so magenta-ish artwork such as
    purple or pink badge fills survives."""
    img = img.convert("RGB")
    w, h = img.size
    src = img.load()
    near = bytearray(w * h)  # within the loose threshold
    for y in range(h):
        for x in range(w):
            if key_dist2(src[x, y]) < 150**2:
                near[y * w + x] = 1
    gone = bytearray(w * h)
    stack = []
    for x in range(w):
        for y in (0, h - 1):
            if near[y * w + x] and not gone[y * w + x]:
                gone[y * w + x] = 1
                stack.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if near[y * w + x] and not gone[y * w + x]:
                gone[y * w + x] = 1
                stack.append((x, y))
    while stack:
        x, y = stack.pop()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and near[ny * w + nx] and not gone[ny * w + nx]:
                gone[ny * w + nx] = 1
                stack.append((nx, ny))
    out = Image.new("RGBA", img.size)
    dst = out.load()
    for y in range(h):
        for x in range(w):
            p = src[x, y]
            if gone[y * w + x] or key_dist2(p) < 70**2:
                dst[x, y] = (0, 0, 0, 0)
            else:
                dst[x, y] = (*p, 255)
    return out


def label_components(alpha):
    w, h = alpha.size
    px = alpha.load()
    seen = bytearray(w * h)
    comps = []
    for sy in range(h):
        for sx in range(w):
            if px[sx, sy] == 0 or seen[sy * w + sx]:
                continue
            stack, pts = [(sx, sy)], []
            seen[sy * w + sx] = 1
            while stack:
                x, y = stack.pop()
                pts.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and px[nx, ny] and not seen[ny * w + nx]:
                        seen[ny * w + nx] = 1
                        stack.append((nx, ny))
            comps.append(pts)
    return comps


def resize_rgba(img, scale):
    size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
    # premultiplied, so transparent pixels don't bleed into the edges
    return img.convert("RGBa").resize(size, Image.BOX).convert("RGBA")


def add_outline(img):
    """Grow the silhouette by 1 px (4-neighbourhood) and paint the new ring dark plum."""
    a = img.getchannel("A").point(lambda v: 255 if v >= 128 else 0)
    w, h = img.size
    ap, ip = a.load(), img.load()
    out = img.copy()
    op = out.load()
    for y in range(h):
        for x in range(w):
            if ap[x, y]:
                op[x, y] = (*ip[x, y][:3], 255)
                continue
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < w and 0 <= ny < h and ap[nx, ny]:
                    op[x, y] = (*OUTLINE, 255)
                    break
            else:
                op[x, y] = (0, 0, 0, 0)
    return out


def slice_sheet(path, cols, rows, canvas, fit, min_area=80, ref=None, outline=True, center=False):
    """Slice by connected component so sprites may cross grid lines; tiny specks are dropped."""
    sheet = alpha_cut(Image.open(path))
    cw, ch = sheet.width / cols, sheet.height / rows
    groups = [[] for _ in range(cols * rows)]
    for pts in label_components(sheet.getchannel("A")):
        if len(pts) < min_area:
            continue
        cx = sum(x for x, _ in pts) / len(pts)
        cy = sum(y for _, y in pts) / len(pts)
        groups[min(rows - 1, int(cy // ch)) * cols + min(cols - 1, int(cx // cw))].append(pts)
    spx = sheet.load()
    crops = []
    for pts_list in groups:
        xs = [x for pts in pts_list for x, _ in pts]
        ys = [y for pts in pts_list for _, y in pts]
        box = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
        crop = Image.new("RGBA", (box[2] - box[0], box[3] - box[1]), (0, 0, 0, 0))
        cp = crop.load()
        for pts in pts_list:
            for x, y in pts:
                cp[x - box[0], y - box[1]] = spx[x, y]
        crops.append(crop)
    dims = sorted(max(c.width, c.height) for c in crops)
    median = dims[len(dims) // 2]
    if ref is None:
        scale = fit / dims[-1]
    else:  # pose sheets: same scale as the reference sheet, corrected for generator size drift
        scale = min(ref[0] * ref[1] / median, fit / dims[-1] * 1.15)
    margin = 3 if outline else 1
    scale = min(scale, (canvas - 2 * margin - 1) / dims[-1])
    frames = []
    for crop in crops:
        small = resize_rgba(crop, scale)
        frame = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
        # characters stand on the bottom edge; icons are centred on both axes
        top = (canvas - small.height) // 2 if center else canvas - margin - 1 - small.height
        frame.paste(small, ((canvas - small.width) // 2, top))
        frames.append(add_outline(frame) if outline else frame)
    return frames, (scale, median)


def indexed_group(frames, colors):
    """One shared palette for the group. Returns (pal565 with index 0 = key, list of index lists, preview frames)."""
    cw, ch = frames[0].size
    mosaic = Image.new("RGB", (cw * len(frames), ch))
    for i, f in enumerate(frames):
        mosaic.paste(f.convert("RGB"), (i * cw, 0), f.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
    pal = mosaic.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    flat = pal.getpalette()[: colors * 3]
    pal565 = [KEY] + [rgb565(flat[i * 3], flat[i * 3 + 1], flat[i * 3 + 2]) for i in range(colors)]
    pal565 = [v if i == 0 or v != KEY else v ^ 0x0020 for i, v in enumerate(pal565)]
    out, prev = [], []
    for f in frames:
        q = f.convert("RGB").quantize(palette=pal, dither=Image.Dither.NONE)
        a = f.getchannel("A")
        qp, ap = q.load(), a.load()
        idx = []
        view = Image.new("RGBA", f.size, (0, 0, 0, 0))
        vp = view.load()
        for y in range(f.height):
            for x in range(f.width):
                if ap[x, y] < 128:
                    idx.append(0)
                else:
                    i = qp[x, y] + 1
                    idx.append(i)
                    c = pal565[i]
                    vp[x, y] = (((c >> 11) & 31) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3, 255)
        out.append(idx)
        prev.append(view)
    return pal565, out, prev


def background(path, colors):
    img = Image.open(path).convert("RGB")
    target = 3 / 4
    if img.width / img.height > target:
        w = round(img.height * target)
        img = img.crop(((img.width - w) // 2, 0, (img.width - w) // 2 + w, img.height))
    else:
        h = round(img.width / target)
        img = img.crop((0, 0, img.width, h))
    img = img.resize((240, 320), Image.LANCZOS)
    q = img.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    flat = q.getpalette()[: colors * 3]
    pal565 = [rgb565(flat[i * 3], flat[i * 3 + 1], flat[i * 3 + 2]) for i in range(colors)]
    return pal565, list(q.getdata()), q.convert("RGB")


def fmt_u16(name, words, per_line=16):
    lines = [f"const uint16_t {name}[{len(words)}] PROGMEM = {{"]
    for i in range(0, len(words), per_line):
        lines.append("  " + ",".join(f"0x{w:04X}" for w in words[i : i + per_line]) + ",")
    lines.append("};\n")
    return "\n".join(lines)


def fmt_u8(name, data, per_line=48):
    lines = [f"const uint8_t {name}[{len(data)}] PROGMEM = {{"]
    for i in range(0, len(data), per_line):
        lines.append("  " + ",".join(str(b) for b in data[i : i + per_line]) + ",")
    lines.append("};\n")
    return "\n".join(lines)


def preview_strip(frames, name, scale, per_row=8):
    os.makedirs(PREVIEW, exist_ok=True)
    cw, ch = frames[0].size
    rows = (len(frames) + per_row - 1) // per_row
    sheet = Image.new("RGBA", (cw * min(per_row, len(frames)), ch * rows), (120, 100, 140, 255))
    for i, f in enumerate(frames):
        sheet.alpha_composite(f, ((i % per_row) * cw, (i // per_row) * ch))
    sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST).save(os.path.join(PREVIEW, name))


def main():
    r = lambda n: os.path.join(RAW, n)
    names16 = None
    ui_names = ["APPLE", "BALL", "MOON", "CLOCK", "PAW", "HEART", "BOWL", "BOLT", "HOUSE", "BAG", "BOOK", "BELL", "BROOM", "CROSS", "STAR", "PAD"]
    item_names = ["RAMEN", "ONIGIRI", "DANGO", "POTION", "MEDICINE", "KUNAI", "SHURIKEN", "SCROLL", "HEADBAND", "POOP", "BANDAGE", "SKULL", "LIGHT", "SMOKE", "COIN", "LANTERN"]
    decor_names = ["SPARKLE", "PLUS", "HEART", "FLOWER", "CLOUD", "BOW", "NOTE", "STAR"]
    stage_names = ["EGG", "CRACK", "BABY", "CHILD", "TEEN", "SICK", "SLEEP", "GHOST"]
    adult_names = ["FOX", "TOAD", "TANUKI", "CAT", "WEASEL", "SLUG", "HAWK", "SNAKE"]
    menu_names = ["FEED", "PLAY", "CLEAN", "BAG", "HOME", "SLEEP", "SHOP", "CLOCK", "DEX"]
    roomui_names = ["BACK", "FOOD", "PLAY", "CLEAN", "HEAL", "BAG", "SLEEP", "MISSION"]

    P = 96
    adults_idle, adults_ref = slice_sheet(r("v2_adults.png"), 4, 2, P, P - 10)
    # a missing pose sheet falls back to the idle sheet so the pipeline can run while art is being generated
    pose = lambda f: slice_sheet(r(f), 4, 2, P, P - 10, ref=adults_ref)[0] if os.path.exists(r(f)) else adults_idle
    groups = [
        ("UI", ui_names, slice_sheet(r("v2_ui.png"), 4, 4, 32, 28, center=True)[0], 96, 32),
        ("ITEM", item_names, slice_sheet(r("v2_items.png"), 4, 4, 32, 28, center=True)[0], 96, 32),
        ("DECOR", decor_names, slice_sheet(r("v2_decor.png"), 4, 2, 24, 22, outline=False, center=True)[0], 24, 24),
        ("MENU", menu_names, slice_sheet(r("v2_menu.png"), 3, 3, 72, 66, center=True)[0], 128, 72),
        ("ROOMUI", roomui_names, slice_sheet(r("v2_roomui.png"), 4, 2, 44, 40, center=True)[0], 96, 44),
        ("STAGE", stage_names, slice_sheet(r("v2_stages.png"), 4, 2, P, P - 10)[0], 96, P),
        ("ADULT", adult_names, adults_idle, 96, P),
        ("HAPPY", adult_names, pose("v2_adults_happy.png"), 96, P),
        ("SAD", adult_names, pose("v2_adults_sad.png"), 96, P),
        ("SLEEPING", adult_names, pose("v2_adults_sleep.png"), 96, P),
    ]
    bgs = [("VILLAGE", "v2_bg_village.png", 96), ("DOJO", "v2_bg_dojo.png", 96), ("CLOCK", "v2_bg_night.png", 96)] + [
        (f"ROOM_{n}", f"v2_room_{n.lower()}.png", 96) for n in ["FEED", "PLAY", "CLEAN", "BAG", "SLEEP"]
    ]

    h = ["// Generated by tools/make_assets.py - do not edit", "#pragma once", "#include <Arduino.h>", "",
         "// Palette-indexed sprite: px[] holds palette indices, index 0 is transparent.", "struct Sprite {", "  const uint8_t *px;", "  const uint16_t *pal;", "  uint8_t w, h;", "};", ""]
    c = ["// Generated by tools/make_assets.py - do not edit", '#include "assets.h"', ""]
    for prefix, names, frames, colors, size in groups:
        pal, idxs, prev = indexed_group(frames, colors)
        preview_strip(prev, f"{prefix.lower()}.png", 3 if size <= 32 else 2)
        h.append(f"#define {prefix}_SIZE {size}")
        h.append(f"enum {prefix.title()}Id {{ " + ", ".join(f"{prefix}_{n}" for n in names) + f", {prefix}_COUNT }};")
        h.append(f"extern const Sprite {prefix}_FRAMES[{prefix}_COUNT];\n")
        c.append(fmt_u16(f"{prefix}_PAL", pal))
        for n, idx in zip(names, idxs):
            c.append(fmt_u8(f"{prefix}_{n}_PX", idx))
        c.append(f"const Sprite {prefix}_FRAMES[{prefix}_COUNT] = {{")
        for n in names:
            c.append(f"  {{ {prefix}_{n}_PX, {prefix}_PAL, {size}, {size} }},")
        c.append("};\n")

    h.append("// Backgrounds: 240x320, palette-indexed")
    for name, file, colors in bgs:
        pal, idx, im = background(r(file), colors)
        os.makedirs(PREVIEW, exist_ok=True)
        im.save(os.path.join(PREVIEW, f"bg_{name.lower()}.png"))
        h.append(f"extern const uint16_t BG_{name}_PAL[{len(pal)}];")
        h.append(f"extern const uint8_t BG_{name}[240 * 320];")
        c.append(fmt_u16(f"BG_{name}_PAL", pal))
        c.append(fmt_u8(f"BG_{name}", idx))

    open(os.path.join(SRC, "assets.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(SRC, "assets.cpp"), "w").write("\n".join(c))
    print("wrote src/assets.h, src/assets.cpp and previews in assets/preview/")


if __name__ == "__main__":
    sys.exit(main())
