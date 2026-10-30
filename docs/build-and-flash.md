# Build and flash

## Requirements

- A CYD board and a USB data cable
- **PlatformIO Core** (CLI) or the PlatformIO IDE extension for VS Code
- Python 3 with **Pillow** only if you want to regenerate sprites or fonts (`pip install pillow`)

Install the PlatformIO CLI in an isolated environment, for example:

```bash
python3 -m venv ~/.platformio-venv
~/.platformio-venv/bin/pip install platformio
export PATH="$HOME/.platformio-venv/bin:$PATH"
```

The **first build downloads the ESP32 toolchains and frameworks, several gigabytes, and can take 20-30 minutes**.
Later builds take seconds to a minute.

## Build and flash

```bash
git clone <this repo> AniESP32
cd AniESP32
pio run                    # compile the default env (cyd2usb)
pio run -t upload          # compile and flash
pio device monitor         # serial log at 115200 baud
```

### Environments

| Env | Display | Notes |
|-----|---------|-------|
| `cyd2usb` (default) | ST7789, BGR, no inversion, no tone curve | Board with micro USB **and** USB-C |
| `cyd` | ILI9341 | Original board; keeps the inversion workaround and the 1.45 tone curve |
| `cyd-fast` | same as `cyd2usb` | `DEBUG_FAST`: the pet's life cycle runs **60x faster**; the DEX header becomes a "next form" cheat |

Pick with `-e`: `pio run -e cyd -t upload`. If you are not sure which board you have, flash the
[display test example](examples/cyd-display-test) first.

### Linux: serial port permission

If upload fails with `Permission denied: '/dev/ttyUSB0'`:

```bash
sudo usermod -aG dialout $USER     # permanent, log out and in again
# or, just for now:
sudo chmod a+rw /dev/ttyUSB0
```

### Uploading tips

- Hold **BOOT** while plugging in if the board is not detected, or if the auto-reset is flaky.
- If flashing fails at 921600 baud, set `upload_speed = 460800` (or 115200) in `platformio.ini`.
- Flash size: the project uses its own `partitions.csv` (3.9 MB app, no OTA). It keeps the NVS area where it was, so
  switching tables does not erase your pet. See [Hardware: flash layout](hardware.md#flash-layout).

## Configuration

### `include/config.h`

| Option | Default | Meaning |
|--------|---------|---------|
| `WIFI_SSID`, `WIFI_PASS` | empty | Wi-Fi used **only** for NTP time sync. Empty = no Wi-Fi, set the clock by hand |
| `TZ_POSIX` | `"PST-8"` | POSIX timezone string. The sign is inverted: UTC+8 is `PST-8`. Example with DST: `"CET-1CEST,M3.5.0,M10.5.0/3"` |
| `TIME_SCALE` | 1 (60 with `DEBUG_FAST`) | Game seconds per real second |
| `USE_LDR` | 1 | Light sensor on/off |
| `COLOR_GAMMA` | 1.45 (1.0 in the ST7789 envs) | Per-channel tone curve applied to every colour. 1.0 = off |

Do not commit real Wi-Fi credentials. Keep a local copy of `config.h` edited, or use `git update-index
--skip-worktree include/config.h`.

### Build flags in `platformio.ini`

| Flag | Meaning |
|------|---------|
| `-DCYD_NO_INVERT` | Skip `tft.invertDisplay(true)` (needed for the ST7789 envs) |
| `-DCOLOR_GAMMA=1.0f` | Override the tone curve for that env |
| `-DDEBUG_FAST` | Fast life cycle for testing |
| `-DSPI_FREQUENCY=...` | Display clock. 27 MHz is the reliable setting here |
| the display and pin flags | Explained in [Display and touch configuration](display-and-touch.md) |

## Clock and time

The board has no battery-backed clock. At boot the time is restored from the last saved value (never earlier than the
firmware's build time). With Wi-Fi configured it syncs over NTP once at boot and whenever you press SYNC on the clock
screen. Without Wi-Fi, set it with the SET, +H and +M buttons; it will restart from the last saved time after a
power cut.

The pet's **day/night** and **alarm** depend on the clock, so set it correctly.

## Testing the game quickly

`pio run -e cyd-fast -t upload` makes everything happen 60 times faster: the egg hatches almost instantly, a baby
grows up in about 30 seconds, and a teen evolves after about 24 minutes. In that build, tapping the header of the DEX
screen jumps the pet to the next adult form so you can see every sprite.

## Rebuilding generated files

```bash
python3 tools/make_assets.py   # assets/v2/raw/*.png  ->  src/assets.h, src/assets.cpp
python3 tools/make_font.py     # tools/fonts/*.ttf    ->  src/font_data.h, src/font_data.cpp
```

Details in [Assets](assets.md).

## Resetting the save data

Everything is stored in the NVS flash partition under the namespace `tama`. To start from scratch, erase it:

```bash
pio run -t erase       # erases ALL flash, you then need to upload again
```

or clear just the keys from code with `Preferences prefs; prefs.begin("tama"); prefs.clear();`.
