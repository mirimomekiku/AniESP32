# Extending the project

Recipes for the common changes. Rebuild assets with `python3 tools/make_assets.py` whenever you touch art.

## Tune the balance

Almost everything is a constant near the top of one file:

| What | Where |
|------|-------|
| Stage lengths, care-mistake grace, sickness death time | top of `src/pet.cpp` (`EGG_TIME`, `BABY_TIME`, `CHILD_TIME`, `TEEN_TIME`, `NEED_GRACE`, `SICK_DEATH`) |
| Food/joy/energy drain and sleep recovery | `petTick()` in `src/pet.cpp` (the `0.010f`, `0.007f`, `0.004f`, `0.03f` factors) |
| Poop interval, tantrum interval | `scheduleNextPoop()`, `scheduleNextTantrum()` |
| Evolution rules | the `EVO[]` table in `src/pet.cpp` |
| Mission list and rewards | `MISSIONS[]` in `src/pet.cpp` |
| Shop items, prices, effects | `SHOP_ITEMS[]` and `useItem()` in `src/inventory.cpp` |
| Ryo rewards | `playerAddRyo(...)` calls in `pet.cpp` |
| Shuriken speed and hit zones | top of `src/games.cpp` |
| Roaming speed and area in rooms | top of `src/room.cpp` |
| Night hours | `clockIsNight()` in `src/clock.cpp`; the day-phase tint in `village.cpp` |
| Dim delay and level | `hwUpdate()` in `src/hw.cpp` |

Use `pio run -e cyd-fast` to test timing changes quickly.

## Add a new adult form

1. Draw the new character in all four sheets (`v2_adults`, `_happy`, `_sad`, `_sleep`), keeping the cell order. The
   grid is 4x2 so replacing an existing form is easy; adding a ninth needs the grid to grow (change the `slice_sheet`
   calls and `adult_names` in `tools/make_assets.py`).
2. Add the name to `adult_names` in `tools/make_assets.py`, run the script.
3. Add the display name in `adultName()` and a row in `EVO[]` in `src/pet.cpp`.
4. Add its hint to `albumHint()` in `src/album.cpp`.

`ADULT_COUNT` and the DEX grid follow automatically (the DEX grid has room for 8 cards, enlarge `CARD_H` or paginate
for more). The DEX bitmask is a `uint8_t`, so more than 8 forms needs it widened.

## Add a room

1. Draw the background (240x320, floor in the lower 40 %) and add `v2_room_<name>.png`.
2. Add the name to the room list in the `bgs` line of `tools/make_assets.py`.
3. In `src/room.cpp` add an entry to `ROOMS[]` (background, palette, up to two bottom-right icons and their
   actions), an enter wrapper (like `feedEnter`) and a `Screen` constant; declare it in `screen.h`, add it to `ScreenId` and the
   `screens[]` array in `main.cpp`.
4. Add a menu badge (`v2_menu.png`) and an entry in `mainmenu.cpp`.

## Add a shop item

1. Add an id before `SI_COUNT` in `src/inventory.h`, a row in `SHOP_ITEMS[]`, and a `case` in `useItem()`.
2. Pick an `ITEM_*` sprite for it (or add one to `v2_items.png`).
3. The shop grid shows 6 cards; for more, reduce `CARD_H` or add paging in `shop.cpp`.

The player save blob has `SI_COUNT` item counters, so bump `PLAYER_VERSION` in `inventory.cpp` when you change it
(this resets ryo and the bag).

## Add a screen

1. Write `enter`, `update`, `touch` and export a `const Screen`.
2. Add it to `screen.h` (`extern` and `ScreenId`) and to the `screens[]` array in `main.cpp` in the same order.
3. Navigate with `gotoScreen(SCR_...)`. For a nav-bar tab, keep it in the first three ids.
4. Give pages without the nav bar a back icon (`ROOMUI_FRAMES[ROOMUI_BACK]`) like the clock and DEX do.

## Change the look

- **Colours:** the UI palette and gradient ramps are set in `gfxInit()` in `src/gfx.cpp` (`R_PINK`, `R_MINT`, ...).
- **Fonts:** see [Assets](assets.md#fonts).
- **Panel style:** `gradPanel()` in `gfx.cpp` (corner radius, band count, highlight).
- **Fade speed:** the two `hwFadeTo` calls in `switchTo()` in `main.cpp`.

## Use a different board

The firmware is written for the CYD's fixed pins. To port it, change:

- the display flags in `platformio.ini` ([guide](display-and-touch.md)),
- the touch pins and calibration in `main.cpp`,
- the pins at the top of `hw.cpp` (LED, speaker, LDR, backlight),
- and the screen size constants in `gfx.h` and the layout constants in each screen (everything is laid out for
  240x320).

## Ideas not yet built

- Friend visits over ESP-NOW between two boards
- A settings screen (sound on/off, brightness, timezone)
- Loading art from the microSD card to free flash
- Extra animation frames for the baby and child stages
