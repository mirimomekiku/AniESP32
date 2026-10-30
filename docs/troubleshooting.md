# Troubleshooting

## Display

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Colours washed out, "wrong" shades, patches that look too green/blue/grey | Wrong controller driver for your panel | Flash the [display test](examples/cyd-display-test) with both envs; use `cyd2usb` (ST7789) or `cyd` (ILI9341) accordingly |
| Everything is a photo negative | Inversion mismatch | Other env, or toggle `TFT_INVERSION_OFF` / `tft.invertDisplay()` |
| Red and blue swapped | Colour order | Toggle `TFT_RGB_ORDER=TFT_BGR` |
| Random wrong-coloured patches or stripes in smooth gradients, flicker | SPI clock too high for your unit | Lower `SPI_FREQUENCY` (55 -> 40 -> 27 MHz) |
| Blank white or black screen, backlight on | Wrong driver or a bad pin flag | Check the pin flags against [hardware](hardware.md); try the other env |
| Backlight never turns on | `TFT_BL` pin or polarity | `TFT_BL=21`, `TFT_BACKLIGHT_ON=HIGH` |
| Screen stays black after a change of screen | Fade is waiting or the backlight is at 0 | Touch the screen; if it persists the transition call crashed, see the serial log |
| Image mirrored or rotated | Rotation | `tft.setRotation(0)` is portrait for both controllers |
| Pastel art still looks pale | Panel gamma | Raise `COLOR_GAMMA` in `include/config.h` (try 1.3 to 1.6) |
| Colours look too dark or contrasty | Tone curve too strong | Lower `COLOR_GAMMA`, `1.0` turns it off |

The hidden **colour test screen** (tap the very top-right corner of the main menu) shows raw bars, channel ramps and
the UI pastels with and without the tone curve; it is the quickest way to see what the panel really renders.

## Touch

| Symptom | Fix |
|---------|-----|
| Taps land mirrored or offset | Recalibrate the four numbers in `readTouch()` in `src/main.cpp`; the [example sketch](examples/cyd-display-test/src/main.cpp) prints raw values at the corners |
| Taps do nothing | The press is detected on the IRQ line; check the touch pins and that the display uses HSPI, touch uses VSPI |
| Double taps | Presses are edge-triggered; a flaky screen may bounce, add a short debounce in `handleTouch()` |

## Upload and boot

| Symptom | Fix |
|---------|-----|
| `Permission denied: /dev/ttyUSB0` | `sudo usermod -aG dialout $USER` and log in again, or `sudo chmod a+rw /dev/ttyUSB0` |
| `Failed to connect` / board not found | Different cable (charge-only cables do not work), hold BOOT while plugging in |
| Upload errors in the middle | `upload_speed = 460800` or `115200` |
| No serial output | Baud rate 115200; the firmware is quiet by default |
| `sprite alloc failed` on the serial log | Not enough contiguous RAM for a view sprite. Close other allocations, shrink the sprite, or reduce what the screen creates |
| Reboot loop | Read the stack trace with `pio device monitor` (the exception decoder is enabled) |

## Game behaviour

| Symptom | Explanation |
|---------|-------------|
| The pet is asleep at the wrong time | The clock is wrong. Night is 21:00-06:00 by the board clock; set it on the clock screen or configure Wi-Fi/NTP |
| The pet sleeps in a lit room | The light sensor may be unreliable on your unit. Check the `LDR` value on the clock screen (`LDR ?` means untrusted); set `USE_LDR 0` to disable |
| The pet keeps going back to sleep | Tap to wake it: it then stays awake until the night or the dark ends |
| Time resets to an old value after a power cut | There is no battery-backed clock; use NTP or set it again. The last saved time is restored |
| New ghost every time | A sick pet needs medicine within 90 minutes. Buy a few in the shop, they are in the bag |
| Everything got reset after flashing | "Erase flash" or a changed save version wipes the save; normal uploads keep it |
| Fast test build changed my pet | `cyd-fast` runs 60x faster; do not play your real pet on it |

## Build

| Symptom | Fix |
|---------|-----|
| First build very slow | It downloads the ESP32 toolchains and frameworks, several GB. One time only |
| `region 'drom0_0_seg' overflowed` or flash overflow | Too much art for the app slot; the project already uses `huge_app.csv`. Reduce colours or sprites in `tools/make_assets.py` |
| `#warning TOUCH_CS pin not defined` | Harmless, the touch controller uses its own library |
| `ledcAttach` not found | Old Arduino-ESP32 core 2.x: the code has a fallback for it, make sure `ESP_ARDUINO_VERSION_MAJOR` is defined (use a recent platform) |
