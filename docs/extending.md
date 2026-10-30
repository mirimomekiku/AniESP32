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

## Add a new adult form (or replace one)

A form is one more character with all 12 poses.

1. Draw it into every sheet of the 5x3 grid (`v3_base.png` and the 11 pose sheets in `assets/v3/raw`), keeping the
   cell order. Easiest is to regenerate the set with the new character included, using the base sheet as the
   reference. The grid has 15 cells (3 growing stages + 12 forms); a 13th form needs a bigger grid, so change the
   `slice_sheet(..., 5, 3, ...)` calls and `CHARS` in `tools/make_assets.py`.
2. Add the name to `CHARS` in `tools/make_assets.py`, run the script. `ADULT_COUNT`, `CH_*` and `PET_SPRITES` update.
3. Add its display name in `adultName()` and a row in the `EVO[]` table in `src/pet.cpp` (first match wins, so put
   it before the more general rows).
4. Add its hint to `albumHint()` in `src/album.cpp` and a ramp to the `ramps[]` table in the DEX drawing code.

The DEX mask is 16 bits, so up to 16 forms work; the DEX shows 6 per page and pages automatically.

## Add a room

1. Draw the background (240x320, floor in the lower 40 %) and add `v2_room_<name>.png`.
2. Add the name to the room list in the `bgs` line of `tools/make_assets.py`.
3. In `src/room.cpp` add an entry to `ROOMS[]` (background, palette, up to two bottom-right icons and their
   actions), an enter wrapper (like `feedEnter`) and a `Screen` constant; declare it in `screen.h`, add it to `ScreenId` and the
   `screens[]` array in `main.cpp`.
4. Add a menu badge (`v2_menu.png`) and an entry in `mainmenu.cpp`.

## Add a shop item

1. Add the id before `SI_COUNT` in `src/inventory.h` and a row in `SHOP_ITEMS[]` with its price, category
   (`IK_FOOD` or `IK_ITEM`), icon and what it gives (food, joy, energy, discipline). Foods use the `FOOD_*` sprites,
   other items the `ITEM_*` sprites (add a new icon to `v3_foods.png` or `v2_items.png`).
2. Add it to `SHOP_FOODS[]` or `SHOP_OTHERS[]` (the arrays also fix the sizes of the two shop categories, update
   `listSize()` in `shop.cpp` if you change the counts). The shop pages itself: four cards per page.
3. Special behaviour (like medicine curing sickness) goes in `useItem()` in `inventory.cpp`.

New ids must be appended: saves store counts by id. Bump `PLAYER_VERSION` and extend the migration in `playerLoad()`
when you change the item count (the current code migrates the original 6-item save).

## Add a screen

1. Write `enter`, `update`, `touch` and export a `const Screen`.
2. Add it to `screen.h` (`extern` and `ScreenId`) and to the `screens[]` array in `main.cpp` in the same order.
3. Navigate with `gotoScreen(SCR_...)`. For a nav-bar tab, keep it in the first three ids.
4. Give pages without the nav bar a back icon (`ROOMUI_FRAMES[ROOMUI_BACK]`) like the clock and DEX do.

## Edit the help text

The help pages are the `PAGES[]` table at the top of `src/help.cpp`: a title and a body of paragraphs separated by
`\n`. Lines wrap automatically; a page has room for about 20 lines of 36 characters. Keep text uppercase to match the
rest of the UI.

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
