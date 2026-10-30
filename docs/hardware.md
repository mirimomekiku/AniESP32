# Hardware

Everything about the board AniESP32 runs on, and how each part is wired and used by the firmware.
For the **display and touch setup you can copy into your own project**, see
[Display and touch configuration](display-and-touch.md).

## The board

| | |
|---|---|
| Name | **ESP32-2432S028**, nicknamed the **Cheap Yellow Display (CYD)** |
| MCU module | ESP32-WROOM-32 (dual core Xtensa, 240 MHz, Wi-Fi + Bluetooth) |
| Flash / RAM | 4 MB flash, ~520 KB SRAM (about 320 KB usable by the app), **no PSRAM** |
| Display | 2.8" TFT, 240x320, 16-bit colour, SPI |
| Touch | XPT2046 resistive controller, SPI |
| On board | RGB LED, speaker amplifier, light sensor (LDR), microSD slot, BOOT/RESET buttons, 3 GPIO connectors |
| Power / programming | USB, CH340 USB-to-serial chip |

Because there is no PSRAM, RAM is the tight resource: a full-screen 16-bit frame buffer would be 150 KB, so the
firmware draws through small "view" sprites instead (see [Architecture](architecture.md#rendering)).

## Which CYD variant do you have?

Two boards carry the same `ESP32-2432S028` label but use **different display controllers**. Using the wrong
driver is the most common source of wrong colours.

| | Original | "2 USB" variant |
|---|---|---|
| USB ports | micro USB only | micro USB **and** USB-C |
| Display controller | **ILI9341** | **ST7789** |
| Colours with the ILI9341 init | correct | washed out, inverted or value-dependent wrong colours |
| This project's env | `cyd` | `cyd2usb` (**default**) |

Quick test, no guessing needed: flash the [display test example](examples/cyd-display-test) with each env. The bars
must read RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA in the right colours. See
[Telling the variants apart](display-and-touch.md#telling-the-variants-apart).

> The USB-C port on the 2-USB variant lacks the CC resistors, so it does not work with USB-C to USB-C cables.
> Use micro USB, or a USB-C to USB-A adapter.

## Full pinout

Pins marked **used** are claimed by this firmware. Everything else is free or reserved by the board.

| GPIO | Function | Used by AniESP32 | Notes |
|------|----------|------------------|-------|
| 2 | TFT DC (RS) | display | Also a strapping pin; fine as wired |
| 12 | TFT MISO | display (HSPI) | Display read-back; not used for drawing |
| 13 | TFT MOSI | display (HSPI) | |
| 14 | TFT SCLK | display (HSPI) | |
| 15 | TFT CS | display (HSPI) | |
| 21 | TFT backlight | **PWM dimming** | LEDC 5 kHz, 8 bit. Also on connector P3 |
| 25 | Touch CLK | touch (VSPI) | |
| 32 | Touch MOSI | touch (VSPI) | |
| 33 | Touch CS | touch | |
| 36 | Touch IRQ | touch | Input only |
| 39 | Touch MISO | touch (VSPI) | Input only |
| 26 | Speaker amplifier input | **buzzer** | `tone()`; the speaker connector is not a GPIO |
| 4 | RGB LED red | alert LED | **Active low** (LOW = on) |
| 16 | RGB LED green | free | Active low |
| 17 | RGB LED blue | free | Active low |
| 34 | Light sensor (LDR) | **dark-room detection** | Input only, ADC1, read at 0 dB attenuation |
| 5, 18, 19, 23 | microSD (SS, SCK, MISO, MOSI) | not used | VSPI, shared with the touch bus choice |
| 0 | BOOT button | not used | Held low at reset to enter flashing mode |
| 35 | Free input | not used | Input only, no pull-ups. Connector P3 |
| 22 | Free | not used | Connectors P3 and CN1 (good for I2C SCL) |
| 27 | Free | not used | Connector CN1 (good for I2C SDA) |
| 1 / 3 | UART0 TX / RX | serial log | Connector P1 |

Display and touch sit on **different SPI buses** (HSPI for the display, VSPI for the touch controller), so touch
reads never fight with screen refreshes.

## Peripherals and how the firmware drives them

All of this lives in [`src/hw.cpp`](../src/hw.cpp).

### Backlight (GPIO 21)

The backlight is driven by an LEDC PWM channel (5 kHz, 8-bit) so the screen can dim and fade:

```cpp
// Arduino-ESP32 core 3.x
ledcAttach(21, 5000, 8);
ledcWrite(21, 255);          // 0..255

// core 2.x
ledcSetup(0, 5000, 8);
ledcAttachPin(21, 0);
ledcWrite(0, 255);
```

The code supports both core versions (`ESP_ARDUINO_VERSION_MAJOR >= 3`). The backlight is used for:

- **Fade through black** on every screen change (`hwFadeTo`)
- **Dimming** after 15 s without touch when it is night, dark, or the pet is asleep (`hwUpdate`)

TFT_eSPI configures GPIO 21 as a plain output at start-up (`TFT_BL` / `TFT_BACKLIGHT_ON`); the firmware then takes it
over for PWM.

### RGB LED (GPIO 4 / 16 / 17)

Active low: `digitalWrite(pin, LOW)` turns the colour **on**.

```cpp
digitalWrite(4, !on);   // red channel
```

The pet blinks the red LED when it is calling for attention, and during the alarm.

### Speaker (GPIO 26)

GPIO 26 feeds the on-board audio amplifier. The firmware just uses the Arduino `tone()` function for short beeps and
jingles (square waves). The 2-pin speaker connector (P4) is wired to the amplifier, not to a GPIO.

```cpp
tone(26, 1175, 120);  // 1175 Hz for 120 ms
```

There is no mute toggle in the UI yet; `soundOn` in `hw.cpp` is the switch.

### Light sensor (GPIO 34)

A photoresistor on GPIO 34. Per the CYD examples, **0 is bright and higher values are darker**, and the reading is
only useful at the most sensitive range:

```cpp
analogSetPinAttenuation(34, ADC_0db);
int raw = analogRead(34);
```

The firmware does not trust the sensor blindly:

1. It averages 4 reads every 250 ms and smooths them.
2. A floating or missing sensor jitters wildly between back-to-back reads; that marks it **unreliable** and the
   feature switches itself off (the clock screen then shows `LDR ?`).
3. It **learns** the room's bright and dark readings and derives thresholds with hysteresis (defaults 2000 dark /
   1600 light until enough range is seen). Learned limits are stored in flash.
4. "Dark" must hold for about 20 s before the pet goes to sleep.

Set `USE_LDR 0` in [`include/config.h`](../include/config.h) to disable it entirely. The raw value is shown on the
clock screen to help you check that your board's sensor works.

### Touch (XPT2046)

See [Display and touch configuration](display-and-touch.md#touch).

### SD card

Not used. The pins are listed above in case you extend the project (for example to load art from the card instead
of flash).

## Flash layout

The firmware uses the `huge_app.csv` partition table:

| Partition | Offset | Size | Use |
|-----------|--------|------|-----|
| nvs | 0x9000 | 20 KB | **Save data** (pet, player, clock, settings) |
| otadata | 0xE000 | 8 KB | unused |
| app0 | 0x10000 | **3 MB** | the firmware |
| spiffs | 0x310000 | 896 KB | unused |
| coredump | 0x3F0000 | 64 KB | |

Why: the generated art (about 1.5 MB of sprites and backgrounds) plus the Wi-Fi stack no longer fit in the default
1.9 MB app slot. The firmware currently uses about 2.67 MB, roughly 85 % of the 3 MB slot. There is no OTA slot.

The **NVS partition sits at the same address in every common partition table**, so switching tables does not erase
your pet's save data. (Flashing with "erase flash" does.)

## Serial

- 115200 baud, `pio device monitor` (with the exception decoder filter enabled)
- Upload speed 921600; if uploads fail, lower `upload_speed` to 460800 or 115200
- Boot messages are mostly silent; the firmware only prints `sprite alloc failed` if a screen cannot allocate its
  view buffer

## Power and heat

USB 5 V is enough. Wi-Fi is only switched on for a few seconds when syncing time (and only if you configured
credentials), then turned off again, so the board stays cool and power draw stays low.
