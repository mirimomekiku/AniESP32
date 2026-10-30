# AniESP32 documentation

| Page | Read it to... |
|------|---------------|
| [Hardware](hardware.md) | know the board, the two display variants, every pin and how each peripheral is used |
| [Display and touch configuration](display-and-touch.md) | **reuse** the TFT_eSPI + XPT2046 setup in your own project (PlatformIO flags, `User_Setup.h`, init code, calibration) |
| [Build and flash](build-and-flash.md) | install the toolchain, pick an environment, configure Wi-Fi and the timezone |
| [Gameplay](gameplay.md) | look up any rule: life cycle, care, evolution table, shop, missions, mini-games |
| [Architecture](architecture.md) | understand the code: modules, screens, rendering, save data |
| [Assets](assets.md) | regenerate sprites and fonts, or make new art that fits |
| [Extending](extending.md) | add forms, rooms, items and screens; tune balance; port to another board |
| [Troubleshooting](troubleshooting.md) | fix colours, touch, upload and memory problems |

## Reusable example

[`examples/cyd-display-test`](examples/cyd-display-test) is a complete, tested PlatformIO project (about 60 lines)
that brings up the display and touch on both CYD variants. Start your own project from it.

```bash
cd docs/examples/cyd-display-test
pio run -e cyd-st7789 -t upload     # two USB ports
pio run -e cyd-ili9341 -t upload    # one USB port
```

It draws colour bars (so you can verify the driver) and plots your touches, printing raw and screen coordinates to the
serial monitor.
