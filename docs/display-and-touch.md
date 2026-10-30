# Display and touch configuration

This page documents exactly how the CYD's display and touch screen are configured in this project, in a form you can
**copy into any other PlatformIO or Arduino project**. A tested, minimal example project lives in
[`docs/examples/cyd-display-test`](examples/cyd-display-test).

- [Summary](#summary)
- [Reusable PlatformIO configuration](#reusable-platformio-configuration)
- [What each flag does](#what-each-flag-does)
- [Arduino IDE: the same thing as a User_Setup.h](#arduino-ide-the-same-thing-as-a-user_setuph)
- [Initialising the display in code](#initialising-the-display-in-code)
- [Touch](#touch)
- [Telling the variants apart](#telling-the-variants-apart)
- [Tuning and performance](#tuning-and-performance)
- [Lessons learned](#lessons-learned)

## Summary

| Setting | ST7789 board (default) | ILI9341 board |
|---------|------------------------|---------------|
| Driver flag | `ST7789_DRIVER` | `ILI9341_2_DRIVER` |
| Colour order | `TFT_RGB_ORDER=TFT_BGR` | default (RGB) |
| Inversion | `TFT_INVERSION_OFF` | none; may need `invertDisplay(true)` on some units |
| Resolution | 240 x 320, portrait is `setRotation(0)` | same |
| Display bus | HSPI: MOSI 13, MISO 12, SCLK 14, CS 15, DC 2, RST none | same |
| Backlight | GPIO 21, active high | same |
| SPI clock | **27 MHz** | 27 MHz (55 MHz is the repo default, see [tuning](#tuning-and-performance)) |
| Touch | XPT2046 on VSPI: CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 | same |
| Library | TFT_eSPI 2.5.x, XPT2046_Touchscreen | same |

## Reusable PlatformIO configuration

Copy this into your `platformio.ini`. Pick **one** of the two environments at the bottom for your board.

```ini
[env]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = huge_app.csv      ; only needed for big firmwares, drop it for small ones
lib_deps =
	bodmer/TFT_eSPI@^2.5.33
	https://github.com/PaulStoffregen/XPT2046_Touchscreen.git#v1.4
build_flags =
	-DUSER_SETUP_LOADED          ; tell TFT_eSPI not to read its own User_Setup.h
	-DUSE_HSPI_PORT              ; the display is on the HSPI bus
	-DTFT_MISO=12
	-DTFT_MOSI=13
	-DTFT_SCLK=14
	-DTFT_CS=15
	-DTFT_DC=2
	-DTFT_RST=-1                 ; reset is tied to the ESP32 reset line
	-DTFT_BL=21
	-DTFT_BACKLIGHT_ON=HIGH
	-DSPI_FREQUENCY=27000000
	-DSPI_READ_FREQUENCY=20000000
	-DSPI_TOUCH_FREQUENCY=2500000
	-DLOAD_GLCD                  ; built-in fonts, drop the ones you do not use to save flash
	-DLOAD_FONT2
	-DLOAD_FONT4

; Board with micro USB + USB-C (ST7789 panel)
[env:cyd-st7789]
build_flags =
	${env.build_flags}
	-DST7789_DRIVER
	-DTFT_RGB_ORDER=TFT_BGR
	-DTFT_INVERSION_OFF

; Original board, micro USB only (ILI9341 panel)
[env:cyd-ili9341]
build_flags =
	${env.build_flags}
	-DILI9341_2_DRIVER
```

This project's own [`platformio.ini`](../platformio.ini) is the same configuration plus `-DCYD_NO_INVERT`,
`-DCOLOR_GAMMA=1.0f` (firmware options, not TFT_eSPI flags) and the extra fonts it loads.

No `User_Setup.h` copying is needed: every setting is a build flag, so a project is self-contained.

## What each flag does

| Flag | Meaning |
|------|---------|
| `USER_SETUP_LOADED` | Tells TFT_eSPI that setup is provided by the build, so it ignores `User_Setup.h` and `User_Setup_Select.h` |
| `ST7789_DRIVER` / `ILI9341_2_DRIVER` | Selects the controller's init sequence. **This must match the panel**, see [variants](#telling-the-variants-apart) |
| `TFT_RGB_ORDER=TFT_BGR` | The ST7789 panel on this board has its colour filters in B-G-R order |
| `TFT_INVERSION_OFF` | Sends "inversion off" after init. The ST7789 init turns inversion **on** by default, which gives a negative image on this board |
| `USE_HSPI_PORT` | Use the ESP32's HSPI peripheral for the display, leaving VSPI free for the touch controller |
| `TFT_MISO/MOSI/SCLK/CS/DC/RST` | Display pins. `TFT_RST=-1` because reset is wired to the chip's reset |
| `TFT_BL`, `TFT_BACKLIGHT_ON` | Backlight pin and the level that turns it on. The firmware later takes the pin over for PWM |
| `SPI_FREQUENCY` | Display write clock in Hz. See [tuning](#tuning-and-performance) |
| `SPI_READ_FREQUENCY` | Clock used when reading pixels back (rarely used) |
| `SPI_TOUCH_FREQUENCY` | Only used by TFT_eSPI's built-in touch support, which this project does not use (the XPT2046 library does the touch) |
| `LOAD_GLCD`, `LOAD_FONT2`... | Compile in TFT_eSPI's built-in fonts. This project draws with its own bitmap fonts, so these are only kept for the diagnostic sketches |

The compiler prints `#warning TOUCH_CS pin not defined`; it is harmless because the touch controller is handled by
a separate library.

## Arduino IDE: the same thing as a User_Setup.h

If you use the Arduino IDE, create or edit `libraries/TFT_eSPI/User_Setup.h` with this content (ST7789 board):

```cpp
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define USE_HSPI_PORT
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF

#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

For the ILI9341 board replace the first three lines with `#define ILI9341_2_DRIVER`. Select "ESP32 Dev Module" as the
board; the CYD community repo also ships ready-made `User_Setup.h` files.

## Initialising the display in code

The order matters. This is what [`gfxInit()`](../src/gfx.cpp) does:

```cpp
tft.init();
tft.setRotation(0);        // portrait: 240 wide, 320 tall
tft.setSwapBytes(true);     // so pushImage() takes standard RGB565 arrays
#ifndef CYD_NO_INVERT
tft.invertDisplay(true);    // ILI9341 boards that show a negative image only
#endif
```

Notes:

- **Rotation 0 is portrait** for both controllers. The other rotations are 1 = landscape,
  2 = portrait upside-down, 3 = landscape flipped.
- **`setSwapBytes(true)`** makes `pushImage` and sprite `pushImage` interpret your `uint16_t` arrays as normal
  `0xRRRRRGGGGGGBBBBB` values. Without it the colours come out scrambled. `fillRect` and friends always take normal
  RGB565 and are unaffected.
- **Colours** are RGB565: `tft.color565(r, g, b)` truncates; this project converts with proper rounding
  (`(r*31+127)/255`, ...) so pastel colours do not drift green.
- **Sprites for flicker-free animation**: create a 16-bit `TFT_eSprite` for the animated area, draw into it, then
  `pushSprite(x, y)`. Keep it small: with no PSRAM, 240x160x2 = 77 KB is about the practical ceiling for one sprite.
  Delete the old sprite before creating another (the firmware keeps exactly one at a time).

```cpp
TFT_eSprite view(&tft);
view.setColorDepth(16);
view.setSwapBytes(true);
view.createSprite(200, 176);
// ... view.fillRect / view.pushImage ...
view.pushSprite(20, 100);
```

## Touch

The touch controller is an **XPT2046** on its own SPI bus (VSPI), with these pins:

| Signal | GPIO |
|--------|------|
| CLK | 25 |
| MOSI | 32 |
| MISO | 39 |
| CS | 33 |
| IRQ | 36 |

Initialisation, as in [`src/main.cpp`](../src/main.cpp):

```cpp
#include <XPT2046_Touchscreen.h>

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(33 /*CS*/, 36 /*IRQ*/);

touchSPI.begin(25 /*CLK*/, 39 /*MISO*/, 32 /*MOSI*/, 33 /*CS*/);
ts.begin(touchSPI);
ts.setRotation(1);
```

### Reading and calibrating

Raw readings are 0..4095. The community-standard calibration for the CYD's landscape orientation (used by its
examples) is:

```cpp
TS_Point p = ts.getPoint();
int xl = map(p.x, 200, 3700, 0, 320);   // landscape x
int yl = map(p.y, 240, 3800, 0, 240);   // landscape y
```

AniESP32 runs in **portrait** (`tft.setRotation(0)`), so it rotates the landscape coordinates:

```cpp
int x = constrain(239 - yl, 0, 239);
int y = constrain(xl,        0, 319);
```

This follows from the controller's rotation table: portrait (`MADCTL = MX`) and landscape (`MADCTL = MV`) differ by a
quarter turn, which gives `x = 239 - yl`, `y = xl`. The same formula is used for both display variants.

If taps land mirrored or offset on your unit, tune the four numbers (`200, 3700, 240, 3800`): print the raw values at
the four screen corners (the [example sketch](examples/cyd-display-test/src/main.cpp) logs them over serial) and
replace the ranges with the minimum and maximum you observe.

### Detecting a press

```cpp
bool touched = ts.tirqTouched() && ts.touched();   // IRQ line first: cheap, then confirm
if (touched && !wasTouched) { /* finger just went down */ }
wasTouched = touched;
```

The firmware acts on the **press edge** only, so a long press is one tap, and it brightens the screen on any
contact.

## Telling the variants apart

Flash the [display test example](examples/cyd-display-test) with the ST7789 env first and read the colour bars:

| What you see | Meaning | Fix |
|--------------|---------|-----|
| Bars match their labels | Correct driver | Done |
| Everything looks like a photo negative | Inversion mismatch | Use the other env, or toggle `TFT_INVERSION_OFF` / `tft.invertDisplay()` |
| Red and blue swapped (RED bar looks blue) | Wrong colour order | Toggle `TFT_RGB_ORDER=TFT_BGR` |
| Colours look washed out, pastel shades wrong, some shades "too green or blue" | **Wrong controller driver** | Use the other env |
| Garbage, shifted or mirrored image | Wrong controller driver or rotation | Use the other env |

The two boards are told apart for sure by the USB ports (one port = ILI9341, two ports = ST7789), but some resellers mix
them, so trust the test.

## Tuning and performance

**SPI clock (`SPI_FREQUENCY`).** The CYD community uses 55 MHz. On the unit this project was developed on, 55 MHz
produced **bit errors** that showed up as random wrong-coloured patches in pale gradients; 40 MHz helped and 27 MHz
was reliable. If you see speckles, shifted colours or flicker, lower the clock before suspecting anything else. 27 MHz
costs a little redraw speed only (a full 200x176 sprite push takes about 21 ms).

**Drawing cost.** The firmware batches pixels into horizontal runs (`fillRect` per run of equal colour) and copies
backgrounds row by row; a typical animated frame is 5-15 ms of CPU plus the push, comfortably inside the 50 ms frame
budget (20 fps).

**Tone curve.** `COLOR_GAMMA` in `include/config.h` applies a per-channel gamma to every colour drawn. It exists as a
workaround for panels that look washed out; with the correct ST7789 driver it is set to `1.0` (off) by the
`cyd2usb` env. See [Architecture: colour handling](architecture.md#colour-handling).

**Gradients on RGB565.** Green has 6 bits but red and blue only 5, so blending pale near-grey colours makes the tint
flip from band to band. `mix565()` therefore snaps the three channels to step together. Keep this in mind if you
write your own gradient code.

## Lessons learned

These cost real debugging time here; they may save you some.

1. **Wrong controller driver = subtle, value-dependent colour errors.** Driving the ST7789 variant with the ILI9341
   init looked "inverted" at first, then "washed out", then produced random green/grey patches. Adding workarounds
   (invert, gamma) made it better but never right. The documented ST7789 settings fixed everything at once.
2. **Do not over-trust 55 MHz SPI.** It works on many units, not all.
3. **Convert colours with rounding**, not truncation, and keep gradients channel-aligned.
4. **Keep the display and touch on separate SPI buses.** It avoids touch-versus-refresh contention entirely.
5. **No PSRAM means small sprites.** Plan the animated areas first, and never allocate two big sprites at once.
