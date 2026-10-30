#include "hw.h"

#include <Preferences.h>

#include "config.h"

static const int PIN_SPEAKER = 26;
static const int PIN_LED_R = 4, PIN_LED_G = 16, PIN_LED_B = 17;
static const int PIN_LDR = 34;
static const int PIN_BL = 21;

bool soundOn = true;
static bool attention = false;

static void led(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, !r);  // active low
  digitalWrite(PIN_LED_G, !g);
  digitalWrite(PIN_LED_B, !b);
}

// ---- backlight (PWM) ----
static int bl = 255;
static uint32_t lastTouch = 0, lastFade = 0;

static void blWrite(int v) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BL, v);
#else
  ledcWrite(0, v);
#endif
}

// ---- light sensor state ----
static float smooth = -1;
static int lo = 4095, hi = 0;
static bool dark = false;
static int noisy = 0;
static uint32_t darkSince = 0, lastLdr = 0, lastAdapt = 0, started = 0;

void hwInit() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  led(false, false, false);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_BL, 5000, 8);
#else
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_BL, 0);
#endif
  blWrite(255);
  lastTouch = millis();

  analogSetPinAttenuation(PIN_LDR, ADC_0db);  // most sensitive range, as in the CYD LDR example
  pinMode(PIN_LDR, INPUT);
  Preferences prefs;
  prefs.begin("tama", true);
  lo = prefs.getInt("ldrLo", 4095);
  hi = prefs.getInt("ldrHi", 0);
  prefs.end();
  started = millis();
}

void hwBeep(int freq, int ms) {
  if (!soundOn) return;
  tone(PIN_SPEAKER, freq, ms);
}

void hwMelody(const int *notes, const int *durations, int count) {
  if (!soundOn) return;
  for (int i = 0; i < count; i++) {
    if (notes[i] > 0) tone(PIN_SPEAKER, notes[i], durations[i]);
    delay(durations[i] + 15);
  }
  noTone(PIN_SPEAKER);
}

void hwAttention(bool on) {
  attention = on;
  if (!on) led(false, false, false);
}

void hwTouched() { lastTouch = millis(); }

static bool wantDimNow = false;

int hwTargetBacklight() { return (wantDimNow && millis() - lastTouch > 15000) ? 35 : 255; }

// Ease the PWM duty from the current level to `level`; the square curve looks even to the eye
void hwFadeTo(int level, int ms) {
  int start = bl;
  int steps = max(1, ms / 8);
  for (int i = 1; i <= steps; i++) {
    float t = (float)i / steps;
    float e = level > start ? t * t : 1.0f - (1.0f - t) * (1.0f - t);  // fade-in accelerates, fade-out decelerates
    blWrite(start + (int)((level - start) * e));
    delay(8);
  }
  bl = level;
  blWrite(bl);
}

void hwUpdate(bool wantDim) {
  wantDimNow = wantDim;
  if (attention) led((millis() / 400) % 2 == 0, false, false);

  // fade the backlight: dim after 15 s without touches when it is dark, night or the pet sleeps
  bool dim = wantDim && millis() - lastTouch > 15000;
  int target = dim ? 35 : 255;
  if (millis() - lastFade >= 20 && bl != target) {
    lastFade = millis();
    bl += constrain(target - bl, -6, 6);
    blWrite(bl);
  }
}

// ---- light sensor ----
void ldrUpdate() {
#if USE_LDR
  uint32_t now = millis();
  if (now - lastLdr < 250) return;
  lastLdr = now;

  int minV = 4095, maxV = 0, sum = 0;
  for (int i = 0; i < 4; i++) {
    int v = analogRead(PIN_LDR);
    minV = min(minV, v);
    maxV = max(maxV, v);
    sum += v;
  }
  // a floating (missing) sensor jitters wildly between back-to-back reads
  if (maxV - minV > 400) noisy = min(noisy + 3, 60);
  else if (noisy > 0) noisy--;

  float raw = sum / 4.0f;
  smooth = smooth < 0 ? raw : smooth * 0.9f + raw * 0.1f;
  if (now - started < 5000) return;  // let it settle before learning

  // learn what "bright" and "dark" look like in this room
  if (smooth < lo) lo = (int)smooth;
  if (smooth > hi) hi = (int)smooth;
  if (now - lastAdapt > 600000UL) {  // slowly forget old extremes
    lastAdapt = now;
    lo += (int)((smooth - lo) * 0.02f);
    hi -= (int)((hi - smooth) * 0.02f);
  }

  bool learned = hi - lo >= 500;
  float darkTh = learned ? lo + 0.65f * (hi - lo) : 2000;
  float lightTh = learned ? lo + 0.45f * (hi - lo) : 1600;
  if (!dark && smooth > darkTh) {
    dark = true;
    darkSince = now;
  } else if (dark && smooth < lightTh) {
    dark = false;
  }
#endif
}

int ldrRaw() { return smooth < 0 ? 0 : (int)smooth; }
bool ldrReliable() { return USE_LDR && noisy < 30; }
bool ldrIsDark() { return ldrReliable() && dark; }
void ldrRange(int &l, int &h) {
  l = lo;
  h = hi;
}

void ldrSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putInt("ldrLo", lo);
  prefs.putInt("ldrHi", hi);
  prefs.end();
}
