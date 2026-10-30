#pragma once

// Optional: set Wi-Fi credentials to sync the clock over NTP (tap SYNC in clock mode,
// it also syncs once at boot). Leave WIFI_SSID empty to set the time by hand.
#define WIFI_SSID ""
#define WIFI_PASS ""

// POSIX timezone string. Philippines = UTC+8, no DST. (Note the sign is inverted in POSIX.)
#define TZ_POSIX "PST-8"

// Build with `pio run -e cyd-fast` to run the pet life cycle 60x faster (testing only)
#ifdef DEBUG_FAST
#define TIME_SCALE 60
#else
#define TIME_SCALE 1
#endif

// Light sensor (LDR on GPIO 34): dark room = lights out. Set to 0 if your board has no LDR or it misbehaves.
#define USE_LDR 1

// Display tone curve. This panel lifts the mid-tones, which makes pastel art look washed out.
// 1.0 = no correction; higher = deeper, more saturated colours (try 1.2 to 1.8).
#ifndef COLOR_GAMMA
#define COLOR_GAMMA 1.45f
#endif
