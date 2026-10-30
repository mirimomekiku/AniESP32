#include "album.h"

#include <Preferences.h>

#include "assets.h"
#include "gfx.h"
#include "pet.h"
#include "config.h"
#include "screen.h"

static uint8_t mask = 0;
static uint16_t raised = 0;

void albumLoad() {
  Preferences prefs;
  prefs.begin("tama", true);
  mask = prefs.getUChar("album", 0);
  raised = prefs.getUShort("raised", 0);
  prefs.end();
}

static void albumSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putUChar("album", mask);
  prefs.putUShort("raised", raised);
  prefs.end();
}

void albumUnlock(uint8_t form) {
  if (form >= ADULT_COUNT) return;
  mask |= 1 << form;
  raised++;
  albumSave();
}

bool albumHas(uint8_t form) { return mask & (1 << form); }
uint16_t albumRaised() { return raised; }

int albumCount() {
  int n = 0;
  for (int i = 0; i < ADULT_COUNT; i++) n += albumHas(i);
  return n;
}

// Mirrors the evolution table in pet.cpp (CM = care mistakes)
const char *albumHint(uint8_t form) {
  static const char *hints[ADULT_COUNT] = {
      "CM<=1  DISCIPLINE 70+  BOND 70+",  // fox
      "CM<=3  DISCIPLINE 70+",            // toad
      "CM 2-3  DISCIPLINE UNDER 70",      // tanuki
      "CM 6+  DISCIPLINE 50+",            // cat
      "CM<=1  DISCIPLINE UNDER 70",       // weasel
      "CM 4-5  ENERGY UNDER 50",          // slug
      "CM 4-5  ENERGY 50+",               // hawk
      "CM 6+  DISCIPLINE UNDER 50",       // snake
  };
  return hints[form];
}

// ---- DEX screen ----
static const int CARD_W = 114, CARD_H = 52, CARD_GAP = 3, GRID_X = 4, GRID_Y = 52;
static const int STRIP_Y = 276;
static int selected = -1;

static void cardRect(int i, int &x, int &y) {
  x = GRID_X + (i % 2) * (CARD_W + CARD_GAP);
  y = GRID_Y + (i / 2) * (CARD_H + CARD_GAP);
}

static void drawHeader() {
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 4);
  gradPanel(&tft, 54, 8, 182, 34, R_BUTTER, 5, 0);
  char buf[32];
  snprintf(buf, sizeof(buf), "DEX %d/%d", albumCount(), ADULT_COUNT);
  drawText(&tft, FONT_CHUNK, buf, 64, 17, 2, C_WHITE, C_OUT, TXT_OUTLINE);
  snprintf(buf, sizeof(buf), "RAISED %u", raised);
  drawText(&tft, FONT_SILK, buf, 228, 20, 1, C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
}

static void drawCard(int i) {
  static const Ramp ramps[ADULT_COUNT] = {R_PEACH, R_MINT, R_LAV, R_PINK, R_BUTTER, R_SKY, R_PEACH, R_MINT};
  int x, y;
  cardRect(i, x, y);
  bool has = albumHas(i);
  bool current = pet.stage == S_ADULT && pet.form == i;
  gradPanel(&tft, x, y, CARD_W, CARD_H, selected == i ? R_BUTTER : (has ? ramps[i] : R_GREY), 5, 0);
  if (has) blitSpriteHalf(&tft, ADULT_FRAMES[i], x + 4, y + 1);
  else blitSilhouetteScaled(&tft, ADULT_FRAMES[i], x + 4, y + 1, 48, 48, mix565(R_GREY.bot, C_OUT, 90));
  drawText(&tft, FONT_SILK, has ? adultName(i) : "???", x + 56, y + 13, 1, C_WHITE, C_OUT, TXT_OUTLINE);
  if (current) drawText(&tft, FONT_SILK, "NOW", x + 56, y + 29, 1, C_TEXT, C_WHITE, TXT_SHADOW);
  else if (has) drawText(&tft, FONT_SILK, "SEEN", x + 56, y + 29, 1, C_TEXT, C_WHITE, TXT_SHADOW);
}

static void drawStrip() {
  gradPanel(&tft, 4, STRIP_Y, 232, 24, R_CREAM, 3, 0);
  if (selected < 0) {
    drawText(&tft, FONT_SILK, "TAP A CARD FOR EVOLUTION HINT", 120, STRIP_Y + 8, 1, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
  } else {
    drawText(&tft, FONT_SILK, albumHas(selected) ? adultName(selected) : "???", 120, STRIP_Y + 4, 1, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
    drawText(&tft, FONT_SILK, albumHint(selected), 120, STRIP_Y + 13, 1, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
  }
}

static void albumEnter() {
  drawGradientBg(R_LAV);
  drawHeader();
  for (int i = 0; i < ADULT_COUNT; i++) drawCard(i);
  drawStrip();
}

static void albumTouch(int x, int y) {
  if (x < 52 && y < 52) {
    gotoScreen(SCR_MENU);
    return;
  }
#ifdef DEBUG_FAST
  // test build only: tap the header to jump the pet to the next adult form
  if (y < GRID_Y) {
    pet.stage = S_ADULT;
    pet.form = albumHas(pet.form) ? (pet.form + 1) % ADULT_COUNT : pet.form;
    albumUnlock(pet.form);
    petSave();
    albumEnter();
    return;
  }
#endif
  for (int i = 0; i < ADULT_COUNT; i++) {
    int cx, cy;
    cardRect(i, cx, cy);
    if (x < cx || x >= cx + CARD_W || y < cy || y >= cy + CARD_H) continue;
    int old = selected;
    selected = i;
    if (old >= 0) drawCard(old);
    drawCard(i);
    drawStrip();
    return;
  }
}

static void albumUpdate() {}

const Screen ALBUM_SCREEN = {albumEnter, albumUpdate, albumTouch};
