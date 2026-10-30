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


def components_of(points):
    """4-connected components of a set of pixel coordinates."""
    left = set(points)
    out = []
    while left:
        seed = left.pop()
        comp, stack = [seed], [seed]
        while stack:
            x, y = stack.pop()
            for n in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if n in left:
                    left.discard(n)
                    comp.append(n)
                    stack.append(n)
        out.append(comp)
    return out


def split_merged(comps, cw, cols):
    """Characters that touch each other come out of the labelling as one blob. A blob wider than 1.5 cells is
    cut at the sparsest column near each cell boundary it crosses."""
    out = []
    for pts in comps:
        xs = [x for x, _ in pts]
        lo, hi = min(xs), max(xs)
        if hi - lo + 1 <= 1.5 * cw:
            out.append(pts)
            continue
        hist = {}
        for x in xs:
            hist[x] = hist.get(x, 0) + 1
        cuts = []
        for b in range(int(lo // cw) + 1, int(hi // cw) + 1):
            if b >= cols:
                break
            # the gap between two neighbouring characters lies somewhere between their cell centres
            a_x, b_x = int((b - 0.5) * cw), int((b + 0.5) * cw)
            cuts.append(min(range(max(lo, a_x), min(hi, b_x) + 1), key=lambda x: (hist.get(x, 0), abs(x - b * cw))))
        pieces = [[] for _ in range(len(cuts) + 1)]
        for x, y in pts:
            k = sum(1 for c in cuts if x > c)
            pieces[k].append((x, y))
        # a cut can leave thin slivers of one character's tail on the other side: re-label so they become
        # separate small pieces that the slicer hands to the body they belong to
        for piece in pieces:
            out.extend(components_of(piece))
    return out


def slice_sheet(path, cols, rows, canvas, fit, min_area=80, ref=None, outline=True, center=False):
    """Slice by connected component so sprites may cross grid lines; tiny specks are dropped."""
    sheet = alpha_cut(Image.open(path))
    cw, ch = sheet.width / cols, sheet.height / rows
    groups = [[] for _ in range(cols * rows)]
    comps = [pts for pts in split_merged(label_components(sheet.getchannel("A")), cw, cols) if len(pts) >= min_area]
    info = []
    for pts in comps:
        xs = [x for x, _ in pts]
        ys = [y for _, y in pts]
        info.append((pts, (min(xs), min(ys), max(xs), max(ys)), len(pts), (sum(xs) / len(pts), sum(ys) / len(pts))))
    # Each cell gets exactly one main body: the largest piece centred in it. Every other piece (detached tail
    # tips, sparkles, bubbles) joins the nearest main body, so nothing near a cell border ends up with the
    # neighbouring character.
    def cell_of(cx, cy):
        return min(rows - 1, int(cy // ch)) * cols + min(cols - 1, int(cx // cw))

    mains = {}
    for entry in sorted(info, key=lambda e: -e[2]):
        mains.setdefault(cell_of(*entry[3]), entry)
    main_list = list(mains.values())
    for entry in info:
        pts, _, _, (cx, cy) = entry
        if any(entry is m for m in main_list):
            groups[cell_of(cx, cy)].append(pts)
            continue

        def dist(m):
            # distance from the piece to the body's centre, in units of the body's own half-size: a long tail
            # tip is "closer" to the big body it belongs to than to a small neighbour whose box overlaps it
            mx0, my0, mx1, my1 = m[1]
            hw, hh = max(1, (mx1 - mx0) / 2), max(1, (my1 - my0) / 2)
            return max(abs(cx - (mx0 + mx1) / 2) / hw, abs(cy - (my0 + my1) / 2) / hh)

        best = min(main_list, key=dist)
        groups[cell_of(*best[3])].append(pts)
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


def rle_encode(idx, w, h):
    """Row-wise run-length encoding: (count, value) byte pairs, runs never cross a row. Value 0 = transparent."""
    out = []
    for y in range(h):
        row = idx[y * w : (y + 1) * w]
        x = 0
        while x < w:
            v, n = row[x], 1
            while x + n < w and row[x + n] == v and n < 255:
                n += 1
            out += [n, v]
            x += n
    return out


def preview_strip(frames, name, scale, per_row=8):
    os.makedirs(PREVIEW, exist_ok=True)
    cw, ch = frames[0].size
    rows = (len(frames) + per_row - 1) // per_row
    sheet = Image.new("RGBA", (cw * min(per_row, len(frames)), ch * rows), (120, 100, 140, 255))
    for i, f in enumerate(frames):
        sheet.alpha_composite(f, ((i % per_row) * cw, (i // per_row) * ch))
    sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST).save(os.path.join(PREVIEW, name))


CHARS = ["BABY", "CHILD", "TEEN", "FOX", "TOAD", "TANUKI", "CAT", "WEASEL", "SLUG", "HAWK", "SNAKE", "RABBIT", "AXOLOTL", "PENGUIN", "SHEEP"]
FIRST_ADULT = 3
# animation poses and the sheet each one is drawn on (same 5x3 character layout as the base sheet)
POSES = [("IDLE_A", "v3_base.png"), ("IDLE_B", "v3_idle2.png"), ("WALK_A", "v3_walk1.png"), ("WALK_B", "v3_walk2.png"),
         ("SLEEP", "v3_sleep.png"), ("SIT", "v3_sit.png"), ("EAT_A", "v3_eat1.png"), ("EAT_B", "v3_eat2.png"),
         ("CLEAN_A", "v3_clean1.png"), ("CLEAN_B", "v3_clean2.png"), ("HAPPY", "v3_happy.png"), ("SAD", "v3_sad.png")]


def main():
    v2 = lambda n: os.path.join(RAW, n)
    v3 = lambda n: os.path.join(ROOT, "assets", "v3", "raw", n)
    ui_names = ["APPLE", "BALL", "MOON", "CLOCK", "PAW", "HEART", "BOWL", "BOLT", "HOUSE", "BAG", "BOOK", "BELL", "BROOM", "CROSS", "STAR", "PAD"]
    item_names = ["RAMEN", "ONIGIRI", "DANGO", "POTION", "MEDICINE", "KUNAI", "SHURIKEN", "SCROLL", "HEADBAND", "POOP", "BANDAGE", "SKULL", "LIGHT", "SMOKE", "COIN", "LANTERN"]
    decor_names = ["SPARKLE", "PLUS", "HEART", "FLOWER", "CLOUD", "BOW", "NOTE", "STAR"]
    food_names = ["ONIGIRI", "RAMEN", "DANGO", "MOCHI", "TEMPURA", "SUSHI", "TAIYAKI", "UDON", "MATCHA", "CAKE", "BENTO", "BERRY"]
    menu_names = ["FEED", "PLAY", "CLEAN", "BAG", "HOME", "SLEEP", "SHOP", "CLOCK", "DEX"]
    roomui_names = ["BACK", "FOOD", "PLAY", "CLEAN", "HEAL", "BAG", "SLEEP", "MISSION"]

    h = ["// Generated by tools/make_assets.py - do not edit", "#pragma once", "#include <Arduino.h>", "",
         "// Palette-indexed sprite, row-wise run-length encoded: (count, value) byte pairs, runs never cross a row.", "// Palette index 0 is transparent.",
         "struct Sprite {", "  const uint8_t *rle;", "  const uint16_t *pal;", "  uint8_t w, h;", "};", "", "#define SPRITE_KEY 0x%04X" % KEY, ""]
    c = ["// Generated by tools/make_assets.py - do not edit", '#include "assets.h"', ""]

    def emit_group(prefix, names, frames, colors, size, enum_name=None):
        pal, idxs, prev = indexed_group(frames, colors)
        preview_strip(prev, f"{prefix.lower()}.png", 3 if size <= 32 else 2)
        h.append(f"#define {prefix}_SIZE {size}")
        h.append(f"enum {enum_name or prefix.title() + 'Id'} {{ " + ", ".join(f"{prefix}_{n}" for n in names) + f", {prefix}_COUNT }};")
        h.append(f"extern const Sprite {prefix}_FRAMES[{prefix}_COUNT];\n")
        c.append(fmt_u16(f"{prefix}_PAL", pal))
        for n, idx in zip(names, idxs):
            c.append(fmt_u8(f"{prefix}_{n}_PX", rle_encode(idx, size, size)))
        c.append(f"const Sprite {prefix}_FRAMES[{prefix}_COUNT] = {{")
        for n in names:
            c.append(f"  {{ {prefix}_{n}_PX, {prefix}_PAL, {size}, {size} }},")
        c.append("};\n")

    emit_group("UI", ui_names, slice_sheet(v2("v2_ui.png"), 4, 4, 32, 28, center=True)[0], 96, 32)
    emit_group("ITEM", item_names, slice_sheet(v2("v2_items.png"), 4, 4, 32, 28, center=True)[0], 96, 32)
    emit_group("DECOR", decor_names, slice_sheet(v2("v2_decor.png"), 4, 2, 24, 22, outline=False, center=True)[0], 24, 24)
    emit_group("MENU", menu_names, slice_sheet(v2("v2_menu.png"), 3, 3, 72, 66, center=True)[0], 128, 72)
    emit_group("ROOMUI", roomui_names, slice_sheet(v2("v2_roomui.png"), 4, 2, 44, 40, center=True)[0], 96, 44)
    # a second menu badge (STATS) and the pixel-art MENU icon for the bottom bar
    extra72 = slice_sheet(v3("v3_extra.png"), 2, 1, 72, 66, center=True)[0]
    extra32 = slice_sheet(v3("v3_extra.png"), 2, 1, 32, 28, center=True)[0]
    help72 = slice_sheet(v3("v3_help.png"), 1, 1, 72, 66, center=True)[0]
    emit_group("MENU2", ["STATS", "HELP"], [extra72[0], help72[0]], 96, 72)
    emit_group("NAV", ["MENU"], [extra32[1]], 48, 32)
    emit_group("FOOD", food_names, slice_sheet(v3("v3_foods.png"), 4, 3, 32, 28, center=True)[0], 96, 32)

    # egg, cracking egg and ghost keep their old art; the growing stages are full characters now
    stage = slice_sheet(v2("v2_stages.png"), 4, 2, 96, 86)[0]
    emit_group("STAGE", ["EGG", "CRACK", "GHOST"], [stage[0], stage[1], stage[7]], 96, 96)

    # the 15 characters x 12 poses
    base_frames, base_ref = slice_sheet(v3("v3_base.png"), 5, 3, 96, 86)
    sheets = []
    for pose, fn in POSES:
        if pose == "IDLE_A":
            sheets.append(base_frames)
        elif os.path.exists(v3(fn)):
            sheets.append(slice_sheet(v3(fn), 5, 3, 96, 86, ref=base_ref)[0])
        else:
            print(f"warning: {fn} missing, using the idle pose for {pose}")
            sheets.append(base_frames)
    h.append("#define PET_SIZE 96")
    h.append("enum CharId { " + ", ".join(f"CH_{n}" for n in CHARS) + ", CH_COUNT };")
    h.append("enum PoseId { " + ", ".join(f"POSE_{n}" for n, _ in POSES) + ", POSE_COUNT };")
    h.append("// The adult forms, in the same order as CH_FOX... (adult form = character - CH_FIRST_ADULT)")
    h.append(f"#define CH_FIRST_ADULT {FIRST_ADULT}")
    h.append("enum AdultId { " + ", ".join(f"ADULT_{n}" for n in CHARS[FIRST_ADULT:]) + ", ADULT_COUNT };")
    h.append("extern const Sprite PET_SPRITES[CH_COUNT][POSE_COUNT];\n")
    pet_rows = []
    for ci, name in enumerate(CHARS):
        frames = [sh[ci] for sh in sheets]
        pal, idxs, prev = indexed_group(frames, 64)
        preview_strip(prev, f"pet_{name.lower()}.png", 1, per_row=12)
        c.append(fmt_u16(f"PET_{name}_PAL", pal))
        row = []
        for (pose, _), idx in zip(POSES, idxs):
            c.append(fmt_u8(f"PET_{name}_{pose}_PX", rle_encode(idx, 96, 96)))
            row.append(f"{{ PET_{name}_{pose}_PX, PET_{name}_PAL, 96, 96 }}")
        pet_rows.append("  { " + ", ".join(row) + " },")
    c.append("const Sprite PET_SPRITES[CH_COUNT][POSE_COUNT] = {\n" + "\n".join(pet_rows) + "\n};\n")
    # one contact sheet of every character and pose for a quick visual check
    contact = Image.new("RGBA", (96 * len(POSES), 96 * len(CHARS)), (120, 100, 140, 255))
    for ci in range(len(CHARS)):
        for pi in range(len(POSES)):
            contact.alpha_composite(sheets[pi][ci], (pi * 96, ci * 96))
    os.makedirs(PREVIEW, exist_ok=True)
    contact.save(os.path.join(PREVIEW, "pets_contact.png"))

    h.append("// Backgrounds: 240x320, palette-indexed")
    bgs = [("VILLAGE", "v2_bg_village.png"), ("DOJO", "v2_bg_dojo.png"), ("CLOCK", "v2_bg_night.png")] + [
        (f"ROOM_{n}", f"v2_room_{n.lower()}.png") for n in ["FEED", "PLAY", "CLEAN", "BAG", "SLEEP"]
    ]
    for name, file in bgs:
        pal, idx, im = background(v2(file), 96)
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
