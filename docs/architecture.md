# Architecture

AniESP32 is a small, single-threaded Arduino firmware: one `loop()` that ticks the simulation, handles touch, and
calls the active screen. The code is split so that **game rules never draw** and **screens never own game rules**.

```
                 +------------------------ main.cpp ------------------------+
                 |  touch -> nav bar / active screen   1 s tick -> petTick() |
                 |  fade transitions    periodic saves   alarm, LDR, sync    |
                 +-----------------------------+---------------+-------------+
                                               |               |
          screens (src/*.cpp)                  |               |   logic (no drawing)
   village  mainmenu  room  shop  album        |               |   pet.cpp    inventory.cpp
   clock  games  colortest  itemmenu           |               |   clock.cpp (time, NTP, alarm)
            |                                  |               |
            +-- petview.cpp (sprite choice, speech, effects, stat bars)
            +-- gfx.cpp (panels, text, sprites, backgrounds, nav, buttons)
            +-- hw.cpp (LED, buzzer, backlight, light sensor)
            +-- assets.cpp / font_data.cpp (generated data)
```

## Source map

| File | Responsibility |
|------|----------------|
| `main.cpp` | `setup()`/`loop()`, touch reading and press detection, screen switching with fades, save timers |
| `screen.h` | The `Screen` interface, `ScreenId` list, `gotoScreen()` |
| `pet.h/.cpp` | The pet simulation: stages, stats, needs, poop, sickness, tantrums, evolution table, missions, actions, save/load |
| `inventory.h/.cpp` | Player-level state that outlives a pet: ryo, bag, shop item table, using items |
| `album.h/.cpp` | DEX unlock bitmask and counters, plus the paged DEX screen |
| `clock.h/.cpp` | Time keeping, NTP, offline catch-up, alarm, and the clock screen |
| `petview.h/.cpp` | Presentation of the pet shared by home and rooms: **pose selection** (idle, walk, sit, sleep, eat, clean, happy, sad), speech bubbles, effects (food drop, soap bubbles, smoke), stat bars, attention calls |
| `village.cpp` | Home screen |
| `mainmenu.cpp` | 3x3 badge menu |
| `room.cpp` | Five room screens (feed, play, clean, bag, sleep): roaming pet and bottom-right icons |
| `itemmenu.h/.cpp` | Pop-up lists with paging: owned foods, bag, play options, missions |
| `shop.cpp` | Shop screen: FOOD / ITEMS chips, 2x2 cards, BACK / NEXT |
| `games.cpp` | Shuriken and shell mini-games |
| `stats.cpp` | Stats screen: vitals, care indicators, advice list |
| `help.cpp` | Help screen: seven pages of word-wrapped text (edit the `PAGES` table to change the content) |
| `colortest.cpp` | Hidden colour test screen |
| `gfx.h/.cpp` | Drawing toolkit, including the reusable BACK / NEXT page bar (`pageNavDraw`, `pageNavHit`) and `ButtonRow`, which has a normal (60 px) and a compact (30 px pill) mode |
| `hw.h/.cpp` | Peripherals: LED, buzzer, backlight fade, light sensor |
| `assets.h/.cpp` | **Generated**: sprites, palettes, backgrounds |
| `font_data.h/.cpp` | **Generated**: bitmap fonts |

## The main loop

```
setup():  touch -> display -> hardware -> clock prefs -> time -> load pet/player/album -> first screen (fade in)
loop():
  1. every elapsed real second: petNight = clock says night; petDark = light sensor says dark;
     run petTick() TIME_SCALE times
  2. every 60 s: save pet, player, time, light-sensor calibration
  3. timeUpdate()      NTP result handling and the alarm
  4. handleTouch()     press edge -> nav bar / back icon / screen
  5. ldrUpdate(); hwUpdate()  light sensor and LED/backlight
  6. screen->update()  draws the active screen
```

Screens are only redrawn when something changes or on a fixed ~20 fps tick, and the loop never blocks except for the
short fade transitions and the few jingles.

## Screens

```cpp
struct Screen {
  void (*enter)();             // full redraw; creates the view sprite
  void (*update)();            // every loop
  void (*touch)(int x, int y); // finger just went down at (x, y)
};
```

| Id | Screen | Reached from |
|----|--------|--------------|
| `SCR_VILLAGE` | Home | Nav bar |
| `SCR_MENU` | Main menu | Nav bar |
| `SCR_CLOCK` | Clock | Nav bar |
| `SCR_SHOP`, `SCR_ALBUM`, `SCR_STATS`, `SCR_HELP` | Shop, DEX, Stats, Help | Menu (own back icon) |
| `SCR_FEED/PLAY/CLEAN/BAG/SLEEP` | Rooms | Menu (back icon) |
| `SCR_SHURIKEN`, `SCR_SHELL` | Mini-games | Play list |
| `SCR_DIAG` | Colour test | Hidden: top-right corner of the menu |

The first three ids are the **nav-bar screens**, in the same order as the `TAB_*` constants, so a tap on the bar
maps to a screen id directly (`x / 80`). Every other screen has no nav bar and provides its own back icon.

`gotoScreen(id)` fades the backlight to black (120 ms), calls the new screen's `enter()` while dark, then fades back
in (240 ms). All navigation goes through it.

## Rendering

### Flicker-free animation

Each animated screen owns a small 16-bit **view sprite** (home 200x176, rooms 240x134, clock 240x160, games 240x150).
Every frame the screen copies the background rows into the sprite, draws the pet and effects on top, and pushes the
sprite to the display in one transfer. Static elements (nav bar, stat bars, buttons) are drawn once directly on the
display and redrawn only when they change. Only one view sprite exists at a time (`gfxMakeView` deletes the old
one), which keeps RAM use at about 16 %.

### Sprites

Sprites are **palette-indexed and run-length encoded**: each row is a list of `(count, palette index)` byte pairs
referencing a per-character or per-group RGB565 palette, index 0 meaning transparent.

```cpp
struct Sprite { const uint8_t *rle; const uint16_t *pal; uint8_t w, h; };
```

The blitters walk the runs directly and draw each one with a single `fillRect`, so detailed art costs one call per
colour run, not per pixel:

| Function | Use |
|----------|-----|
| `blitSprite` | 1:1 |
| `blitSpriteScaled` | Nearest-neighbour to any size, optional horizontal flip (squash and stretch, small roaming pet, icons at other sizes) |
| `blitSpriteHalf` | Smooth 2:1 reduction (DEX thumbnails, small pet), optional flip |
| `blitSilhouette[Scaled]` | Single-colour shape (locked DEX entries) |

Pets are 96x96 (15 characters x 12 poses in `PET_SPRITES[character][pose]`), items, foods and UI icons 32x32, menu badges 72x72, room icons 44x44, decorations 24x24. Backgrounds are
240x320 palette-indexed (96 colours) and drawn row by row; the village palette is **tinted** per time of day before
drawing (day, dusk, night, dawn).

### Panels, text, bars

- `gradPanel` draws a rounded panel with a 2 px outline and a **stepped gradient** in a handful of bands, a glossy
  highlight and an optional 3D lip. It works on the display or on any sprite.
- Text uses two **bitmap fonts** (`drawTextC` centres text on the height of a capital letter, which is how labels sit in the middle of buttons) rendered from open fonts: `FONT_SILK` (labels) and `FONT_CHUNK` (numbers, headings),
  with integer scaling, an 8-neighbour **outline** or a drop shadow, and left/centre/right alignment.
- Stat bars are 5 gradient segments; the room screens use a compact version.

### Colour handling

- `rgb(r,g,b)` converts 8-bit colours to RGB565 **with rounding** and then through the tone curve.
- `fx()` applies the tone curve (per-channel lookup tables built from `COLOR_GAMMA`) to palette colours when they
  are drawn. With `COLOR_GAMMA` 1.0 it is the identity.
- `mix565()` blends two colours and snaps the result so red, green and blue step together, which prevents the tint
  flipping between gradient bands (green has 6 bits, red and blue 5).

## Pet simulation

`pet.cpp` is deliberately free of drawing and hardware. It exposes a global `Pet pet` plus actions
(`petEat`, `petPlay`, `petClean`, `petHeal`, `petToggleSleep`, `petTap`, `petStartMission`, ...) that return an
`ActionResult`, and emits `PetEvent`s through a hook (`petEventHook`) when something happens (hatched, evolved, poop,
sick, died, woke up...). `petview.cpp` installs the hook and turns events into sounds and speech bubbles.

`petTick()` advances one game second: vitals, poop timer, sickness, tantrums, need tracking and care mistakes,
mission countdown, growth. The same function is used to **catch up** offline time, with the hook disabled.

Inputs from the outside world are plain globals set by `main.cpp` each second: `petNight` and `petDark`.

## Save data

Stored in the ESP32's NVS flash under the namespace **`tama`** with `Preferences`.

| Key | Type | Content |
|-----|------|---------|
| `pet` | blob | The whole `Pet` struct. Version byte = `SAVE_VERSION` (currently 2); a size or version mismatch discards it and starts a new egg |
| `mission` | blob | Whether a mission is running, which one, seconds left |
| `player` | blob | Ryo and the bag (15 item counters). Version 2; a version-1 blob (6 items) is migrated on load |
| `albumw`, `raised` | ushort, ushort | DEX unlock bitmask (12 bits; older saves used an 8-bit `album` key, still read), adults raised |
| `epoch` | ulong | Last saved time (the board has no RTC) |
| `h24`, `alH`, `alM`, `alOn` | settings | 24 h display, alarm hour, minute, enabled |
| `ldrLo`, `ldrHi` | int | Learned light-sensor range |

Saves happen every 60 seconds and after each action. Changing the `Pet` struct requires bumping `SAVE_VERSION`
(this resets the pet but keeps ryo, bag and DEX, which are in separate keys).

## Memory budget

| | |
|---|---|
| RAM | about 63 KB static of 320 KB; the largest runtime allocation is a 77 KB view sprite |
| Flash | about 2.67 MB of the 3 MB app slot (85 %): roughly 1.1 MB code and libraries, 1.5 MB generated art |
| Sprites | 180 pet frames (RLE, about 2-4 KB each), backgrounds 8 x 77 KB raw |

## Hardware access

All pin access is in `hw.cpp` and `main.cpp` (touch). See [Hardware](hardware.md). The rest of the code uses the
small API in `hw.h` (`hwBeep`, `hwMelody`, `hwAttention`, `hwFadeTo`, `ldrIsDark`, ...).
