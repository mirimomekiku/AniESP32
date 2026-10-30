// Home screen: the pet in its village. Stat bars on top, nav bar at the bottom.
#include "clock.h"
#include "config.h"
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "pet.h"
#include "petview.h"
#include "screen.h"

static const int VX = 20, VY = 100, VW = 200, VH = 176;  // animated scene window: screen y 100..276
static const int FLOOR_Y = 166;                          // view y where the pet's feet rest
static uint32_t lastFrame = 0;

// ---- day / night: the palette of the village background is tinted by the clock ----
static uint16_t tintPal[sizeof(BG_VILLAGE_PAL) / sizeof(uint16_t)];
static int tintPhase = -1;

static int phaseNow() {
  int h = clockHour();
  if (h >= 7 && h < 17) return 0;   // day
  if (h >= 17 && h < 19) return 1;  // dusk
  if (h >= 5 && h < 7) return 3;    // dawn
  return 2;                         // night
}

static void buildTint(int phase) {
  // multipliers per phase, out of 256: r, g, b
  static const int mul[4][3] = {{256, 256, 256}, {256, 200, 160}, {110, 124, 200}, {256, 220, 210}};
  const int *m = mul[phase];
  for (size_t i = 0; i < sizeof(tintPal) / sizeof(uint16_t); i++) {
    uint16_t c = BG_VILLAGE_PAL[i];
    int r = ((c >> 11) & 31) * m[0] / 256;
    int g = ((c >> 5) & 63) * m[1] / 256;
    int b = (c & 31) * m[2] / 256;
    tintPal[i] = (r << 11) | (g << 5) | b;
  }
  tintPhase = phase;
}

void villageInit() { pvInit(); }

static void redrawAll() {
  drawBg(BG_VILLAGE, tintPal);
  pvHudDraw(false, 2, true);
  drawNav(TAB_HOME);
}

static void villageEnter() {
  gfxMakeView(VW, VH);
  buildTint(phaseNow());
  redrawAll();
}

static void drawPetView() {
  uint32_t now = millis();
  copyBgToView(BG_VILLAGE, tintPal, VX, VY, VW, VH);

  if (mission.active) {
    // the pet is away: empty path, a lantern and the countdown
    float t = now / 1000.0f;
    blitSpriteScaled(&view, ITEM_FRAMES[ITEM_LANTERN], VW / 2 - 32, 34 + (int)(sinf(t * 2.0f) * 4), 64, 64);
    char buf[16];
    uint32_t left = mission.left / TIME_SCALE;
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(left / 60), (unsigned long)(left % 60));
    drawText(&view, FONT_CHUNK, buf, VW / 2, 112, 3, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
    drawText(&view, FONT_SILK, MISSIONS[mission.kind].name, VW / 2, 146, 2, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
    view.pushSprite(VX, VY);
    return;
  }

  int bob, dx;
  float sy;
  const Sprite *sprite = pvPickSprite(bob, dx, sy);
  // squash and stretch around the feet: height * sy, width / sy keeps the volume
  int dh = (int)(sprite->h * sy), dw = (int)(sprite->w / sy);
  int px = (VW - dw) / 2 + dx;
  int py = FLOOR_Y + 2 - bob - dh;

  // floor shadow (shrinks while hopping): the ground colour under the pet, darkened
  uint16_t ground = view.readPixel(VW / 2, FLOOR_Y + 4);
  view.fillEllipse(VW / 2, FLOOR_Y, 38 - bob / 3, 7, mix565(ground, C_OUT, 70));

  // poop piles on the ground, behind the pet's feet
  static const int poopPos[4][2] = {{4, 122}, {152, 122}, {38, 132}, {112, 132}};
  for (int i = 0; i < pet.poop && i < 4; i++) blitSpriteScaled(&view, ITEM_FRAMES[ITEM_POOP], poopPos[i][0], poopPos[i][1], 40, 40);

  blitSpriteScaled(&view, *sprite, px, py, dw, dh);

  // status overlays
  if (pet.stage != S_EGG && pet.stage != S_GHOST) {
    if (pet.sleeping) {
      float t = now / 1000.0f;
      int off = (int)(fmodf(t, 3.0f) * 7);
      drawText(&view, FONT_CHUNK, "z", VW / 2 + 44, 66 - off, 2, R_LAV.bot, C_WHITE, TXT_OUTLINE);
      drawText(&view, FONT_CHUNK, "Z", VW / 2 + 58, 44 - off, 3, R_PINK.bot, C_WHITE, TXT_OUTLINE);
    } else if ((pet.sick || pet.food < 1) && pet.stage != S_TEEN) {
      blitSpriteScaled(&view, ITEM_FRAMES[ITEM_BANDAGE], VW / 2 + 18, py + 14, 40, 40);
    }
  }

  pvDrawEffects(&view, VW / 2, FLOOR_Y, 96);
  pvDrawNeedBubble(&view, VW);
  pvDrawBubble(&view, VW);

  // caption: form name, care mistakes, ryo
  char cap[40];
  drawText(&view, FONT_SILK, formName(), 4, 4, 1, C_WHITE, C_OUT, TXT_OUTLINE);
  snprintf(cap, sizeof(cap), "CM %d   %u RYO", pet.careMistakes, player.ryo);
  drawText(&view, FONT_SILK, cap, 4, 16, 1, C_WHITE, C_OUT, TXT_OUTLINE);

  view.pushSprite(VX, VY);
}

static void villageTouch(int x, int y) {
  // tapping the pet pets it, warms an egg or scolds a tantrum
  if (y >= HUD_Y + HUD_H && y < NAV_Y) {
    petTap();
    if (pet.stage == S_EGG) pvSay(pet.warmth >= 70 ? "Cracking!" : "Warm...", 900);
    else if (pet.stage != S_GHOST) pvSay(pet.tantrum ? "Hmph!" : "Hehe!", 900);
    petSave();
  }
}

static void villageUpdate() {
  uint32_t now = millis();

  // the sky follows the clock
  static uint32_t lastPhaseCheck = 0;
  if (now - lastPhaseCheck > 5000) {
    lastPhaseCheck = now;
    int ph = phaseNow();
    if (ph != tintPhase) {
      buildTint(ph);
      redrawAll();
    }
  }

  pvHudDraw(false, 2, false);
  pvUpdateCalls();

  if (now - lastFrame >= 50) {  // ~20 fps
    lastFrame = now;
    drawPetView();
  }
}

const Screen VILLAGE_SCREEN = {villageEnter, villageUpdate, villageTouch};
