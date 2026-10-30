# Assets

All art and fonts are converted at build-preparation time into C arrays stored in flash. Nothing is loaded from
files at runtime.

```
assets/v2/raw/*.png   --tools/make_assets.py-->   src/assets.h, src/assets.cpp
tools/fonts/*.ttf     --tools/make_font.py--->     src/font_data.h, src/font_data.cpp
```

Both outputs are committed so the firmware builds without Python. Run the scripts only when you change art or fonts
(`pip install pillow`, then `python3 tools/make_assets.py`).

## Source sheets

Sprite sheets are drawn on a **flat magenta (#FF00FF) background** in an equal-cell grid. Backgrounds are plain
3:4 portrait images.

| File | Grid | Content | Final size |
|------|------|---------|------------|
| `v2_stages.png` | 4 x 2 | egg, cracking egg, baby, child, teen, sick, sleeping, ghost | 96x96 |
| `v2_adults.png` | 4 x 2 | the 8 adult forms (idle) | 96x96 |
| `v2_adults_happy.png` | 4 x 2 | same 8, happy pose | 96x96 |
| `v2_adults_sad.png` | 4 x 2 | same 8, sad/unwell pose | 96x96 |
| `v2_adults_sleep.png` | 4 x 2 | same 8, sleeping pose | 96x96 |
| `v2_items.png` | 4 x 4 | ramen, onigiri, dango, potion, medicine, kunai, shuriken, scroll, headband, poop, bandage, skull, light bulb, smoke, coin, lantern | 32x32 |
| `v2_ui.png` | 4 x 4 | apple, ball, moon, clock, paw, heart, bowl, bolt, house, bag, book, bell, broom, cross, star, pad | 32x32 |
| `v2_decor.png` | 4 x 2 | sparkle, plus sparkle, heart, flower, cloud, bow, note, star | 24x24 |
| `v2_menu.png` | 3 x 3 | round menu badges: feed, play, clean, bag, home, sleep, shop, clock, dex | 72x72 |
| `v2_roomui.png` | 4 x 2 | round icons: back, food, play, clean, heal, bag, sleep, mission | 44x44 |
| `v2_bg_village.png`, `v2_bg_dojo.png`, `v2_bg_night.png` | - | home, mini-game dojo, clock night sky | 240x320 |
| `v2_room_feed/play/clean/bag/sleep.png` | - | the five room backgrounds | 240x320 |

The **order of cells matters**: it defines the generated enum names (see below). The menu sheet has a `HOME` badge
that is currently not shown in the menu; it is kept so the sheet layout stays stable.

## What the converter does

1. **Remove the background.** Pixels very close to magenta are cleared everywhere; looser matches (anti-aliased
   edges) are cleared only when connected to the sheet border. This keeps magenta-ish artwork, such as purple and
   pink badge fills, intact.
2. **Slice by connected component.** Each sprite is found as a group of connected pixels and assigned to a cell by
   its centre, so artwork may cross cell borders (tails, wings). Specks smaller than 80 px are dropped.
3. **Scale.** One shared scale per sheet keeps relative sizes (all 8 adults use the same factor). The pose sheets are
   scaled to match the idle sheet. Characters are **bottom-aligned**, icons are **centred** in their canvas.
4. **Outline.** A 1 px dark-plum ring is added around every silhouette (except decorations), for the emphasised
   outline look.
5. **Quantise.** One shared palette per group (96 colours for characters, items, UI and room icons, 128 for menu
   badges, 24 for decorations), **no dithering**, so gradients become clean bands.
6. **Write C.** Palettes as `uint16_t` RGB565 (with rounding), pixels as `uint8_t` indices, index 0 = transparent.
7. **Backgrounds** are resized to 240x320 and quantised to 96 colours.

### Generated API

For a group `UI` with names `APPLE`, `BALL`, ... the header contains:

```cpp
#define UI_SIZE 32
enum UiId { UI_APPLE, UI_BALL, ..., UI_COUNT };
extern const Sprite UI_FRAMES[UI_COUNT];
```

Same pattern for `ITEM`, `DECOR`, `MENU`, `ROOMUI`, `STAGE`, `ADULT`, `HAPPY`, `SAD`, `SLEEPING`. Backgrounds:

```cpp
extern const uint8_t  BG_VILLAGE[240 * 320];
extern const uint16_t BG_VILLAGE_PAL[N];
```

Use them with `drawBg(BG_VILLAGE, BG_VILLAGE_PAL)` or `copyBgToView(...)`.

## Fonts

`tools/make_font.py` renders **Silkscreen Bold** (labels) and **Press Start 2P** (numbers, headings) at 8 px with
Pillow, thresholds them to 1 bit and stores one byte per glyph row plus a per-glyph advance, for ASCII 32-126.
Both fonts are under the SIL Open Font License 1.1 (see `tools/fonts/README.md`).

To add a font: drop the TTF into `tools/fonts/`, add an entry to `FONTS` in the script and use the generated
`FONT_<NAME>` in `drawText`. Glyphs wider than 8 px are not supported by this renderer.

## The art direction

The art was generated with an image model from a fixed style brief, then processed as above. To produce matching new
sheets, reuse this brief:

> Vibrant yet soft pastel-leaning colours (pink, lavender, mint, sky blue, butter yellow, cream). Thick clean
> dark-plum outlines (about 2-3 px at final size), continuous and closed, around every character, icon and object.
> Soft cel shading made of 3-4 clearly visible smooth gradient **bands** per material (clean stepped colour ramps,
> no dithering noise, no blur, no airbrushing). Top-left light with a subtle lighter rim. Big glossy eyes with white
> highlights, rosy cheeks, kawaii proportions. Polished high-detail "HD pixel art" quality. No text, letters or
> numbers.

Practical tips that worked:

- Generate a whole **evolution line or character set in one sheet** so the designs stay consistent, then generate the
  pose sheets **using the finished idle sheet as a reference image**.
- Ask for the flat magenta background and strict equal cells; keep the subject fully inside its cell.
- Check licensing and terms of the image tool before redistributing generated art.

## Previewing

The converter writes upscaled previews to `assets/preview/` (git-ignored): `adult.png`, `stage.png`, `ui.png`,
`menu.png`, `bg_*.png`... Open them after every run; they show exactly the quantised result that ends up on the
device.
