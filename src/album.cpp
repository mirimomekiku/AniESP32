#include "album.h"

#include <Preferences.h>

#include "assets.h"
#include "gfx.h"
#include "pet.h"
#include "config.h"
#include "screen.h"

static uint32_t mask = 0;  // one bit per adult form (22 forms)
static uint16_t raised = 0;

void albumLoad() {
  Preferences prefs;
  prefs.begin("tama", true);
  // older saves stored 8 bits under "album" and then 16 under "albumw"
  mask = prefs.getUInt("albumd", prefs.getUShort("albumw", prefs.getUChar("album", 0)));
  raised = prefs.getUShort("raised", 0);
  prefs.end();
}

static void albumSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putUInt("albumd", mask);
  prefs.putUShort("raised", raised);
  prefs.end();
}

void albumUnlock(uint8_t form) {
  if (form >= ADULT_COUNT) return;
  mask |= 1u << form;
  raised++;
  albumSave();
}

bool albumHas(uint8_t form) { return mask & (1u << form); }
uint16_t albumRaised() { return raised; }

int albumCount() {
  int n = 0;
  for (int i = 0; i < ADULT_COUNT; i++) n += albumHas(i);
  return n;
}

// Mirrors the evolution table in pet.cpp (CM = care mistakes, D = discipline, B = bond, J = joy, E = energy)
const char *albumHint(uint8_t form) {
  static const char *hints[ADULT_COUNT] = {
      "CM<=1  D 70+  B 70+",               // fox
      "CM<=3  D 70+  (OTHERWISE)",         // toad
      "CM 2-3  D<70  JOY<70",              // tanuki
      "CM 6+  D 50+  ENERGY<60",           // cat
      "CM<=1  D<70  B<60  JOY<70",         // weasel
      "CM 4-5  ENERGY, JOY, FOOD LOW",     // slug
      "CM 4-5  ENERGY 50-69",              // hawk
      "CM 6+  D<50  B<40",                 // snake
      "CM<=1  D<70  B 60+  JOY 70+",       // rabbit
      "CM 2-3  D<70  JOY 70+  B<50",       // axolotl
      "CM 4-5  ENERGY 70+",                // penguin
      "CM 2-3  D 70+  B 50+",              // sheep
      "CM 6+  D 50+  ENERGY 60+",          // red panda
      "NO MISTAKES  D 85+  B 85+  JOY 80+",// dragon
      "CM<=1  D<70  B<60  JOY 70+",        // koi
      "CM<=1  D 70+  B<70  ENERGY 70+",    // crane
      "CM 4-5  FOOD 60+  JOY<50  E<50",    // hedgehog
      "CM 2-3  D<70  JOY 70+  B 50+",      // capybara
      "CM 4-5  JOY 50+  ENERGY<50",        // octopus
      "CM 2-3  D 70+  B<50  FOOD 70+",     // tortoise
      "CM<=1  D<70  B 60+  JOY<70",        // panda
      "CM 6+  D<50  B 40+",                // bat
  };
  return hints[form];
}

// ---- DEX screen ----
// 12 forms, 6 per page (2 columns x 3 rows), BACK / NEXT along the bottom
static const int CARD_W = 114, CARD_H = 64, CARD_GAP = 3, GRID_X = 4, GRID_Y = 48;
static const int PER_PAGE = 6;
static const int STRIP_Y = 248;
static int selected = -1;
static int pageNo = 0;
static int pageCount() { return (ADULT_COUNT + PER_PAGE - 1) / PER_PAGE; }

static void cardRect(int slot, int &x, int &y) {
  x = GRID_X + (slot % 2) * (CARD_W + CARD_GAP);
  y = GRID_Y + (slot / 2) * (CARD_H + CARD_GAP);
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

// Two short lines for a name such as "FROST PENGUIN" so it fits beside the thumbnail
static void drawName(const char *name, int x, int y, uint16_t fill, uint16_t edge) {
  const char *sp = strchr(name, ' ');
  if (!sp || textWidth(FONT_SILK, name, 1) <= 56) {
    drawText(&tft, FONT_SILK, name, x, y + 4, 1, fill, edge, TXT_OUTLINE);
    return;
  }
  char first[16] = {0};
  strncpy(first, name, min<size_t>(sp - name, 15));
  drawText(&tft, FONT_SILK, first, x, y, 1, fill, edge, TXT_OUTLINE);
  drawText(&tft, FONT_SILK, sp + 1, x, y + 10, 1, fill, edge, TXT_OUTLINE);
}

static void drawCard(int slot) {
  static const Ramp ramps[8] = {R_PEACH, R_MINT, R_LAV, R_PINK, R_BUTTER, R_SKY, R_MINT, R_PEACH};
  int form = pageNo * PER_PAGE + slot;
  if (form >= ADULT_COUNT) return;
  int x, y;
  cardRect(slot, x, y);
  bool has = albumHas(form);
  bool current = pet.stage == S_ADULT && pet.form == form;
  const Sprite &spr = PET_SPRITES[CH_FIRST_ADULT + form][POSE_IDLE_A];
  gradPanel(&tft, x, y, CARD_W, CARD_H, selected == form ? R_BUTTER : (has ? ramps[form % 8] : R_GREY), 5, 0);
  if (has) blitSpriteHalf(&tft, spr, x + 4, y + 8);
  else blitSilhouetteScaled(&tft, spr, x + 4, y + 8, 48, 48, mix565(R_GREY.bot, C_OUT, 90));
  if (has) drawName(adultName(form), x + 56, y + 10, C_WHITE, C_OUT);
  else drawText(&tft, FONT_SILK, "???", x + 56, y + 14, 1, C_WHITE, C_OUT, TXT_OUTLINE);
  if (current) drawText(&tft, FONT_SILK, "NOW", x + 56, y + 44, 1, C_TEXT, C_WHITE, TXT_SHADOW);
  else if (has) drawText(&tft, FONT_SILK, "SEEN", x + 56, y + 44, 1, C_TEXT, C_WHITE, TXT_SHADOW);
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

static void drawAll() {
  drawGradientBg(R_LAV);
  drawHeader();
  for (int i = 0; i < PER_PAGE; i++) drawCard(i);
  drawStrip();
  pageNavDraw(pageNo, pageCount());
}

static void albumEnter() {
  pageNo = 0;
  selected = -1;
  drawAll();
}

static void albumTouch(int x, int y) {
  if (x < 52 && y < 52) {
    gotoScreen(SCR_MENU);
    return;
  }
  int nav = pageNavHit(x, y, pageNo, pageCount());
  if (nav >= 0) {
    pageNo += nav == 0 ? -1 : 1;
    selected = -1;
    fadeThrough(drawAll);
    return;
  }
#ifdef DEBUG_FAST
  // test build only: tap the header to jump the pet to the next adult form
  if (y < GRID_Y) {
    pet.stage = S_ADULT;
    pet.form = albumHas(pet.form) ? (pet.form + 1) % ADULT_COUNT : pet.form;
    albumUnlock(pet.form);
    petSave();
    drawAll();
    return;
  }
#endif
  for (int slot = 0; slot < PER_PAGE; slot++) {
    int form = pageNo * PER_PAGE + slot;
    int cx, cy;
    cardRect(slot, cx, cy);
    if (form >= ADULT_COUNT || x < cx || x >= cx + CARD_W || y < cy || y >= cy + CARD_H) continue;
    int old = selected;
    selected = form;
    if (old >= pageNo * PER_PAGE && old < (pageNo + 1) * PER_PAGE) drawCard(old - pageNo * PER_PAGE);
    drawCard(slot);
    drawStrip();
    return;
  }
}

static void albumUpdate() {}

const Screen ALBUM_SCREEN = {albumEnter, albumUpdate, albumTouch};
