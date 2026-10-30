# AniESP32

A pixel-art, ninja-themed Tamagotchi-style virtual pet with a digital clock, for the
**ESP32-2432S028 "Cheap Yellow Display" (CYD)**: 240x320 portrait touchscreen, speaker, RGB LED, light sensor.

## Features

- Life cycle: egg, baby, child, teen, then one of 8 adult forms decided by how well you cared for it
- Care: food, joy, energy, poop, sickness, tantrums, care mistakes, sleep; day/night and a lights-out light sensor
- Main menu of round badge buttons; room screens (feed, play, clean, bag, sleep) with a small roaming pet
- Shop with ryo coins, a bag of items, a DEX album, two mini-games (shuriken, shell game) and timed missions
- Pixel clock with NTP sync (optional) and an alarm
- Soft banded-gradient pixel UI, bitmap pixel fonts, fades through black between screens

## Build and flash

[PlatformIO](https://platformio.org) is required.

```
pio run -t upload            # ST7789 board (micro USB + USB-C) - the default
pio run -e cyd -t upload     # original ILI9341 board
pio run -e cyd-fast -t upload  # ST7789 board, life cycle 60x faster (testing)
```

On Linux you may need access to the serial port (`sudo usermod -aG dialout $USER`, then log in again).
Set Wi-Fi credentials and your timezone in `include/config.h` for NTP time sync.
Display tuning (SPI speed, driver, tone curve) is in `platformio.ini` and `include/config.h`.

## Layout

```
src/        firmware (pet logic, screens, graphics, hardware)
include/    config.h
tools/      make_assets.py (sprite sheets -> C arrays), make_font.py (fonts -> bitmap tables), fonts/
assets/v2/raw/   source art sheets and backgrounds (AI-generated, see PLAN.md)
PLAN.md     design notes and milestones
```

`src/assets.cpp` and `src/font_data.cpp` are generated; rebuild them with
`python3 tools/make_assets.py` and `python3 tools/make_font.py` (needs Pillow).

## Credits

Fonts: Silkscreen and Press Start 2P (SIL OFL 1.1), see `tools/fonts/README.md`.
Art sources were generated with an image model and then processed by `tools/make_assets.py`.
