# Ninja Tamagotchi (Uni-style) for the CYD: implementation plan

Goal: grow the current Tamagotchi + clock firmware into a Tamagotchi Uni-style game on the ESP32-2432S028
(240x320 portrait, resistive touch, speaker, RGB LED, LDR, SD slot, Wi-Fi). The theme is a hidden ninja village
with original animal-ninja creatures (headbands, scrolls, kunai, scarves). Characters are inspired by the
genre, not copies of existing characters.

The Uni feature mapping below is from general knowledge of the product, not from a spec. Treat it as a design
reference, not a requirement.

## Status

- **M0 done:** modules (`pet`, `gfx`, `clock`, `village`, `hw`), new asset pipeline, indexed backgrounds.
- **M1 done (untested on hardware):** egg to adult, poop, sickness, care mistakes, tantrums, evolution table, save/load.
- **M2 done (untested on hardware):** shop, ryo, bag, FEED and BAG menus, day/night tint, pet sleeps at night.
- **M3 done (untested on hardware):** DEX album, 3 extra adult poses (happy, sad, sleeping), squash-and-stretch animation.
- **M4 done (untested on hardware):** shuriken and shell mini-games, missions (errand, scout, training).
- **M5 done (untested on hardware):** LDR lights-out sleep, backlight dimming, clock alarm. LDR presence not verified.
- **M6 skipped by request** (ESP-NOW friend visits, needs a second CYD). Plan complete.

## Art direction v2

Restyled to a softer, more vibrant look (style references are not included in this repo): thick plum outlines, banded gradient
shading, 96 px sprites, palette-indexed assets, banded-gradient UI panels and a bitmap pixel font (Silkscreen and
Press Start 2P, both SIL OFL). Regenerate everything with `tools/make_font.py` and `tools/make_assets.py`.

## Screen structure (v2.1)

- **Nav bar (bottom):** HOME, SHOP, MENU, TIME, DEX.
- **HOME:** stat bars on top, the pet in its village, no action buttons.
- **MENU:** 3x3 round badges: FEED, PLAY, CLEAN, BAG, HOME, SLEEP, SHOP, CLOCK, DEX.
- **Rooms (FEED, PLAY, CLEAN, BAG, SLEEP):** small roaming pet, back icon top-left, action icons bottom-right.
- **Transitions:** every screen switch fades through black (backlight PWM).

## Display note

The board in use is the ST7789 variant of the CYD. Driving it with the ILI9341 init gave washed-out and
value-dependent wrong colors; the default env is now `cyd2usb` (ST7789, BGR, no inversion, no tone curve).
`pio run -e cyd` builds for the original ILI9341 board instead.

## 1. Design

### Life cycle

```
Egg -> Baby -> Child -> Teen -> Adult (8 forms) -> (optional) Retire / Legacy egg
                                   \-> Ghost (neglect)
```

| Stage | Duration (real time) | Notes |
|-------|----------------------|-------|
| Egg   | 2 min                | tap to warm it, then it hatches |
| Baby  | 30 min               | feed only, no discipline yet |
| Child | 6 h                  | first care checks, poop appears |
| Teen  | 24 h                 | evolution is decided at the end |
| Adult | open-ended           | missions, mini-games, visits |

Timings are constants in one place, with a `DEBUG_FAST` build flag (x60) for testing.

### Stats and care

- **Hunger, Joy, Energy** (the current three). Add **Health** (hidden), **Discipline** (0-100) and **Bond** (0-100).
- **Poop:** appears every ~40-90 min and stays until cleaned. Too much poop causes sickness.
- **Sickness:** from poop, hunger 0 or a missed medicine. Shown with the sick sprite, cured by the medicine item.
- **Care mistakes:** an ignored call (hungry, bored or sleepy) for more than 15 min counts as one. Evolution reads this counter.
- **Attention call:** the RGB LED blinks (GPIO 4/16/17, active low) and the speaker beeps (GPIO 26 via the amp).
- **Light switch:** the pet sleeps on a schedule, using the clock for day/night and the LDR (GPIO 34) to detect a dark room.

### Evolution rules (draft)

Decided when the teen stage ends, using care mistakes (CM), discipline (D) and bond (B):

| Condition | Adult |
|-----------|-------|
| CM <= 1, D >= 70, B >= 70 | Nine-tail fox (best) |
| CM <= 1, D < 70 | Lightning weasel |
| CM 2-3, D >= 70 | Sage toad |
| CM 2-3, D < 70 | Sand tanuki |
| CM 4-5, high Energy | Hawk scout |
| CM 4-5, low Energy | Healer slug |
| CM >= 6, D >= 50 | Shadow cat |
| CM >= 6, D < 50 | Serpent |

The table is data, not code, so it can be balanced without touching the engine.

### Uni-inspired features

1. **Village screen:** the main room, a day/night tint from the clock, and weather from the NTP date (season).
2. **Ramen shop:** earn **ryo** coins and buy food, medicine and cosmetic headbands.
3. **Missions / "school":** timed activities (training, scouting) that raise stats while you do other things.
4. **Mini-games:** shuriken timing (tap on the moving target), the shell game (find the real pet among shadow clones), and a reflex dodge.
5. **Friend visits:** two CYDs exchange a visit over **ESP-NOW** (no router needed). This is the offline stand-in for Uni's online world. Optional later: a Wi-Fi leaderboard.
6. **Clock mode:** keep the existing pixel clock, add an alarm that the pet reacts to.
7. **Album:** adult forms you have unlocked.

## 2. Screens and UX (portrait 240x320)

```
+--------------------------+
| VILLAGE | CLOCK | MENU   |  top tabs (32 px)
| HUD: food joy energy     |  3 pixel bars + status icons (poop, sick, call)
|                          |
|        [ pet ]           |  animated view window (reuses the sprite view)
|                          |
| [FEED] [PLAY] [CLEAN]    |  bottom action row, 60 px
+--------------------------+
MENU: Shop, Missions, Games, Album, Friends, Settings
```

Touch uses the same pixel panel style already in `main.cpp`. A long press on the pet is "pet it" (raises Bond).

## 3. Software architecture

Split `main.cpp` before it grows past ~1000 lines:

```
src/
  main.cpp              setup/loop, scheduler
  core/pet.{h,cpp}      stats, stage, tick(dt), evolution
  core/evolution.cpp    rule table
  core/save.{h,cpp}     versioned blob in NVS (Preferences) + CRC, offline catch-up
  core/clock.{h,cpp}    time, NTP, alarm
  gfx/sprites.{h,cpp}   indexed-sprite blitter with palette + transparency
  gfx/pixelui.{h,cpp}   panel, button, bar, text (from main.cpp today)
  ui/screens/*.cpp      village, shop, missions, album, settings
  games/*.cpp           shuriken, shellgame, dodge
  net/espnow.cpp        friend visits
  hw/alerts.cpp         LED, buzzer, LDR
include/config.h        Wi-Fi, timezone, debug flags
```

- **Game loop:** fixed 1 s logic tick, ~20 fps render. Screens implement `enter/update/draw/touch`.
- **Offline catch-up:** save `epoch` every few minutes. On boot, simulate the elapsed time capped at 8 h, so the pet doesn't die while the device is off.
- **Save safety:** write only on state changes plus a 60 s timer, and use a version byte for migrations.
- **Sound:** I2S DAC on GPIO 26 for short chiptune beeps (generated square waves, no files).

## 4. Asset pipeline and memory budget

Raw art is generated with Codex imagegen into `assets/uni/raw/`. `tools/make_assets.py` is extended to:

1. slice the sprite sheets (grid, magenta key) and normalize frame sizes,
2. quantize to a **shared 16-color palette per character** and store as **4-bit indexed** data,
3. store backgrounds on the 120x160 pixel grid (indexed, 2x at draw time),
4. emit `assets.cpp` plus a manifest (names, sizes, palettes).

Budget (4 MB flash, today 1.4 MB used with the `min_spiffs` layout):

| Asset | Size now (RGB565) | Indexed 4-bit |
|-------|-------------------|---------------|
| 48x48 frame | 4.6 KB | 1.2 KB |
| 8 adults x 6 frames | 221 KB | 55 KB |
| 16 items 28x28 | 25 KB | 6 KB |
| 3 backgrounds 240x320 | 460 KB | 29 KB (at 120x160, 4-bit) |

Use a custom partition table (no OTA, about 2.5 MB app) or move large art to the **SD card** (the CYD has a slot) if you want much more.
The indexed blitter converts palette entries to RGB565 on the fly into the view sprite.

Animation needs per character: idle x2, happy, sad, sleep, eat, sick, plus walk/attack for missions (about 8-10 frames).
Codex sheets give one pose per cell, so extra frames come from further image prompts or from code-side squash/bob tricks (already used in the current build).

## 5. Milestones

| # | Milestone | Done when |
|---|-----------|-----------|
| M0 | Refactor into modules, add indexed sprites, bigger partition | current game looks identical |
| M1 | Life stages and Fox line (egg -> adult), poop, sickness, care mistakes | a full life plays in DEBUG_FAST |
| M2 | Items, shop, ryo, inventory, bg_village + day/night | can buy and use food and medicine |
| M3 | Evolution table and the other 7 adults, album | all 8 forms reachable |
| M4 | Mini-games (shuriken first) and missions | games pay out ryo and stats |
| M5 | LED, buzzer, LDR sleep, alarm | device calls for attention and sleeps in the dark |
| M6 | ESP-NOW friend visits | two CYDs see each other's pets |

## 6. Risks and open questions

- **Touch:** portrait calibration is untested on the board and affects every screen. Verify early.
- **Art consistency:** AI sheets vary between generations. Generate a whole evolution line in one sheet per request and keep prompts fixed.
- **Frames:** more animation frames need more generations, so budget a few Codex runs per character.
- **Palette:** backgrounds can be re-tinted by `retheme()` in the script, so the yellow/green look is a flag, not a re-generation.
- **IP:** keep designs original (no existing characters, symbols or monuments). Every prompt states this.
- **Flash wear:** NVS writes are wear-levelled, but keep the write rate low.
