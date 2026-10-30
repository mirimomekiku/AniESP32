// Stats screen: every number that matters for the pet, and what to do about it.
#include "config.h"
#include "gfx.h"
#include "pet.h"
#include "screen.h"

static uint32_t lastCheck = 0, shownSig = 0;

static Ramp levelRamp(float v) { return v >= 60 ? R_MINT : (v >= 30 ? R_BUTTER : R_RED); }
static const char *levelWord(float v) { return v >= 60 ? "GOOD" : (v >= 30 ? "LOW" : "CRITICAL"); }

// Horizontal bar: dark outline, grey track, banded gradient fill
static void hbar(int x, int y, int w, int h, float v, Ramp r) {
  tft.fillRect(x, y, w, h, C_OUT);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, mix565(R_GREY.top, R_GREY.bot, 128));
  int fw = (int)((w - 4) * constrain(v, 0, 100) / 100.0f);
  for (int row = 0; row < h - 4; row++) {
    int band = row * 3 / (h - 4);
    if (fw > 0) tft.fillRect(x + 2, y + 2 + row, fw, 1, mix565(r.top, r.bot, band * 128));
  }
}

// One of the three vital bars: icon, name, level word, bar and percentage
static void vitalRow(int y, const Sprite &icon, const char *name, float v) {
  Ramp lr = levelRamp(v);
  gradPanel(&tft, 4, y, 232, 34, R_CREAM, 4, 0);
  blitSprite(&tft, icon, 8, y + 1);
  drawTextC(&tft, FONT_SILK, name, 44, y + 11, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);
  drawTextC(&tft, FONT_SILK, levelWord(v), 44, y + 24, 1, lr.bot, C_WHITE, TXT_LEFTX | TXT_SHADOW);
  hbar(104, y + 10, 92, 14, v, lr);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)v);
  drawTextC(&tft, FONT_CHUNK, buf, 230, y + 17, 1, C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
}

static void careBar(int y, const char *name, float v) {
  drawTextC(&tft, FONT_SILK, name, 12, y + 7, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);
  hbar(92, y + 1, 104, 12, v, v >= 50 ? R_SKY : R_LAV);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int)v);
  drawTextC(&tft, FONT_CHUNK, buf, 228, y + 7, 1, C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
}

static void chip(int x, int y, int w, const char *text, Ramp r) {
  gradPanel(&tft, x, y, w, 22, r, 3, 0, false, 3);
  drawTextC(&tft, FONT_SILK, text, x + w / 2, y + 11, 1, C_WHITE, C_OUT, TXT_OUTLINE);
}

static void formatTime(char *out, size_t n, uint32_t gameSeconds) {
  uint32_t s = gameSeconds / TIME_SCALE;
  if (s >= 3600) snprintf(out, n, "%luH %02luM", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
  else snprintf(out, n, "%luM %02luS", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// Up to four lines of advice, most urgent first
static int buildAdvice(char out[4][40]) {
  int n = 0;
  auto add = [&](const char *fmt, int arg = 0) {
    if (n < 4) snprintf(out[n++], 40, fmt, arg);
  };
  if (pet.stage == S_EGG) { add("TAP THE EGG TO WARM IT"); return n; }
  if (pet.stage == S_GHOST) { add("TAP THE SPIRIT FOR A NEW EGG"); return n; }
  if (mission.active) { add("ON A MISSION, IT WILL BE BACK SOON"); return n; }

  if (pet.sick) add("GIVE MEDICINE: IT FEELS ILL");
  if (pet.food < 15) add("FEED IT NOW: FOOD IS CRITICAL");
  else if (pet.food < 30) add("FEED IT: FOOD IS LOW");
  if (pet.poop >= 2) add("CLEAN UP THE POOP (%d PILES)", pet.poop);
  if (pet.tantrum) add("TAP IT TO SCOLD THE TANTRUM");
  if (pet.joy < 15) add("PLAY NOW: IT IS VERY SAD");
  else if (pet.joy < 30) add("PLAY WITH IT: JOY IS LOW");
  if (pet.sleeping) add("LET IT SLEEP: ENERGY RECOVERS");
  else if (pet.energy < 20) add("PUT IT TO BED: ENERGY IS LOW");
  if (n < 4 && pet.food >= 30 && pet.food < 60) add("FEED SOON: FOOD IS DROPPING");
  if (n < 4 && pet.joy >= 30 && pet.joy < 60) add("PLAY SOON: JOY IS DROPPING");
  if (n < 4 && !pet.sleeping && pet.energy >= 20 && pet.energy < 40) add("REST SOON: ENERGY IS DROPPING");
  if (n < 4 && pet.careMistakes >= 3) add("ANSWER ITS CALLS FASTER (%d MISTAKES)", pet.careMistakes);
  if (n < 4 && pet.stage >= S_CHILD && pet.discipline < 50) add("RAISE DISCIPLINE: SCROLLS, SCOLDING");
  if (n == 0) {
    add("ALL GOOD! KEEP IT UP");
    if (pet.stage < S_ADULT) add("TO EVOLVE WELL: FEW MISTAKES");
    if (pet.stage < S_ADULT) add("AND DISCIPLINE 70+");
  }
  return n;
}

static void drawAll() {
  drawGradientBg(R_LAV);
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 4);

  // header: who, and how long until the next stage
  gradPanel(&tft, 54, 8, 182, 34, R_BUTTER, 5, 0);
  drawTextC(&tft, FONT_SILK, formName(), 64, 19, 2, C_WHITE, C_OUT, TXT_LEFTX | TXT_OUTLINE);
  char info[32];
  uint32_t left = petStageSecondsLeft();
  if (left > 0) {
    char t[16];
    formatTime(t, sizeof(t), left);
    snprintf(info, sizeof(info), "NEXT STAGE IN %s", t);
  } else {
    snprintf(info, sizeof(info), "STAGE: %s", stageName());
  }
  drawTextC(&tft, FONT_SILK, info, 64, 34, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);

  // the three vitals
  vitalRow(50, UI_FRAMES[UI_APPLE], "FOOD", pet.food);
  vitalRow(86, UI_FRAMES[UI_HEART], "JOY", pet.joy);
  vitalRow(122, UI_FRAMES[UI_BOLT], "ENERGY", pet.energy);

  // long-term care
  gradPanel(&tft, 4, 160, 232, 64, R_CREAM, 4, 0);
  careBar(165, "DISCIPLINE", pet.discipline);
  careBar(182, "BOND", pet.bond);
  chip(10, 199, 70, pet.sick ? "ILL!" : "HEALTHY", pet.sick ? R_RED : R_MINT);
  char buf[16];
  snprintf(buf, sizeof(buf), pet.poop ? "%d POOP" : "CLEAN", pet.poop);
  chip(85, 199, 70, buf, pet.poop >= 2 ? R_PEACH : R_MINT);
  snprintf(buf, sizeof(buf), "MISTAKES %d", pet.careMistakes);
  chip(160, 199, 70, buf, pet.careMistakes >= 4 ? R_RED : (pet.careMistakes >= 2 ? R_BUTTER : R_MINT));

  // what to focus on
  gradPanel(&tft, 4, 228, 232, 88, R_BUTTER, 5, 0);
  drawTextC(&tft, FONT_SILK, "WHAT TO DO", 14, 240, 2, C_WHITE, C_OUT, TXT_LEFTX | TXT_OUTLINE);
  char adv[4][40];
  int n = buildAdvice(adv);
  for (int i = 0; i < n; i++) {
    tft.fillRect(12, 256 + i * 13 + 2, 4, 4, C_OUT);  // little pixel bullet
    drawTextC(&tft, FONT_SILK, adv[i], 21, 258 + i * 13, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);
  }
}

// A cheap fingerprint of everything shown, so the screen is only redrawn when something changed
static uint32_t signature() {
  uint32_t sig = (uint32_t)pet.food * 31u + (uint32_t)pet.joy * 17u + (uint32_t)pet.energy * 13u + (uint32_t)pet.discipline * 7u + (uint32_t)pet.bond * 5u;
  sig = sig * 31u + pet.poop * 3u + pet.careMistakes * 11u + pet.sick * 2u + pet.sleeping + pet.tantrum * 5u + pet.stage * 19u;
  return sig * 131u + petStageSecondsLeft() / (60u * TIME_SCALE);
}

static void statsEnter() {
  drawAll();
  shownSig = signature();
  lastCheck = millis();
}

static void statsUpdate() {
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();
  uint32_t sig = signature();
  if (sig == shownSig) return;
  shownSig = sig;
  drawAll();
}

static void statsTouch(int x, int y) {
  if (x < 52 && y < 52) gotoScreen(SCR_MENU);
}

const Screen STATS_SCREEN = {statsEnter, statsUpdate, statsTouch};
