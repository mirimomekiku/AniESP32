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
3:4 portrait images. Pets and foods live in `assets/v3/raw`, everything else in `assets/v2/raw`.

### The 15 characters and their 12 poses

The 15 characters, in grid order (5 columns x 3 rows, left to right, top to bottom): **baby, child, teen** (the fox
growing line), then the **12 adult forms**: kitsune, sage toad, tanuki, shadow cat, raiju, moon slug, hawk, serpent,
moon rabbit, axolotl, frost penguin, cloud sheep.

Every pose is its own sheet with the **same 5 x 3 layout**:

| Sheet | Pose |
|-------|------|
| `v3_base.png` | `IDLE_A`: the neutral front-facing pose (also the style reference for all others) |
| `v3_idle2.png` | `IDLE_B`: blink frame |
| `v3_walk1.png`, `v3_walk2.png` | `WALK_A`, `WALK_B`: walking right, opposite legs forward |
| `v3_sleep.png` | `SLEEP` |
| `v3_sit.png` | `SIT` |
| `v3_eat1.png`, `v3_eat2.png` | `EAT_A` (mouth wide open), `EAT_B` (chewing) |
| `v3_clean1.png`, `v3_clean2.png` | `CLEAN_A` (lathered), `CLEAN_B` (shaking off water) |
| `v3_happy.png`, `v3_sad.png` | `HAPPY`, `SAD` |

All 96x96, 180 frames in total.

### Everything else

| File | Grid | Content | Final size |
|------|------|---------|------------|
| `v3_help.png` | 1 x 1 | the HELP menu badge | 72 |
| `v3_extra.png` | 2 x 1 | the STATS menu badge (72x72) and the MENU icon for the bottom bar (32x32) | 72 / 32 |
| `v3_foods.png` | 4 x 3 | onigiri, ramen, dango, mochi, tempura, sushi, taiyaki, udon, matcha, cake, bento, berry | 32x32 |
| `v2_stages.png` | 4 x 2 | only the egg, cracking egg and ghost are used (cells 1, 2 and 8) | 96x96 |
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
2. **Slice by connected component.** Each sprite is found as a group of connected pixels, so artwork may cross cell
   borders (tails, wings). Three rules keep neighbours from stealing each other's pixels:
   - a blob wider than 1.5 cells (two characters that touch) is **split** at the sparsest column between the two
     cell centres;
   - each cell gets exactly **one main body** (the largest piece centred in it);
   - every other piece (detached tail tips, sparkles, soap bubbles) joins the nearest main body, measured in units of
     that body's own size. Specks under 80 px are dropped.
3. **Scale.** One shared scale per sheet keeps relative sizes (all characters use the same factor). Pose sheets are
   scaled to match the base sheet. Characters are **bottom-aligned**, icons are **centred** in their canvas.
4. **Outline.** A 1 px dark-plum ring is added around every silhouette (except decorations), for the emphasised
   outline look.
5. **Quantise.** One palette per group, **no dithering**, so gradients become clean bands. Each character gets its
   **own 64-colour palette** shared by its 12 poses; icon groups use 96 colours, menu badges 128, decorations 24.
6. **Write C.** Palettes as `uint16_t` RGB565 (with rounding). Pixels are stored **run-length encoded**, row by row
   as `(count, index)` byte pairs, with index 0 = transparent. The blitters draw straight from the runs.
7. **Backgrounds** are resized to 240x320 and quantised to 96 colours (stored raw).

The converter also writes `assets/preview/pets_contact.png`, a contact sheet of all 15 characters x 12 poses. Open it
after every run to check the slicing.

### Generated API

For an icon group such as `UI` (names `APPLE`, `BALL`, ...) the header contains:

```cpp
#define UI_SIZE 32
enum UiId { UI_APPLE, UI_BALL, ..., UI_COUNT };
extern const Sprite UI_FRAMES[UI_COUNT];
```

Same pattern for `ITEM`, `FOOD`, `DECOR`, `MENU`, `ROOMUI` and `STAGE` (egg, crack, ghost). The characters:

```cpp
enum CharId { CH_BABY, CH_CHILD, CH_TEEN, CH_FOX, CH_TOAD, ... CH_SHEEP, CH_COUNT };
enum PoseId { POSE_IDLE_A, POSE_IDLE_B, POSE_WALK_A, POSE_WALK_B, POSE_SLEEP, POSE_SIT,
              POSE_EAT_A, POSE_EAT_B, POSE_CLEAN_A, POSE_CLEAN_B, POSE_HAPPY, POSE_SAD, POSE_COUNT };
enum AdultId { ADULT_FOX, ADULT_TOAD, ... ADULT_SHEEP, ADULT_COUNT };   // adult form = character - CH_FIRST_ADULT
extern const Sprite PET_SPRITES[CH_COUNT][POSE_COUNT];
```

Backgrounds:

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

- Generate a whole **character set in one sheet** so the designs stay consistent, then generate each pose sheet
  **using the finished base sheet as a reference image**. Ask the model to keep the same grid positions.
- Keep neighbouring characters from touching; the converter can split small overlaps but clean gaps are best.
- Ask for the flat magenta background and strict equal cells; keep the subject fully inside its cell.
- Check licensing and terms of the image tool before redistributing generated art.

## Previewing

The converter writes upscaled previews to `assets/preview/` (git-ignored): `adult.png`, `stage.png`, `ui.png`,
`menu.png`, `bg_*.png`... Open them after every run; they show exactly the quantised result that ends up on the
device.
