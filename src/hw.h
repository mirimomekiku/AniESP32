// CYD peripherals: RGB LED (active low), speaker amp on GPIO 26, light sensor on GPIO 34, backlight on GPIO 21.
#pragma once
#include <Arduino.h>

void hwInit();
void hwBeep(int freq, int ms);
void hwMelody(const int *notes, const int *durations, int count);  // blocking, keep it short
void hwAttention(bool on);  // blinks the LED while on
void hwUpdate(bool wantDim);  // LED blink and backlight fade; dim = low backlight unless touched recently
void hwFadeTo(int level, int ms);  // blocking smooth backlight fade, 0..255 (used for screen transitions)
int hwTargetBacklight();          // the brightness the screen should settle at right now
void hwTouched();             // call on every touch: brightens the screen for a while
extern bool soundOn;

// Light sensor. Raw ADC at 0 dB: 0 is bright, higher is darker (per the CYD examples).
void ldrUpdate();
int ldrRaw();                    // smoothed reading
bool ldrIsDark();                // false when the sensor is missing or unreliable
bool ldrReliable();
void ldrRange(int &lo, int &hi); // learned range, hi < lo until something was observed
void ldrSave();
