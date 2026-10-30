// Shop screen: spend ryo on food, medicine and training scrolls.
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "screen.h"

static const int CARD_W = 114, CARD_H = 74, CARD_GAP = 3;
static const int GRID_X = 4, GRID_Y = 52;
static const int HEADER_Y = 8, HEADER_X = 54, HEADER_W = 182, HEADER_H = 34;

static const char *toast = "";
static uint32_t toastUntil = 0;
static bool toastShown = false;

static void cardRect(int i, int &x, int &y) {
  x = GRID_X + (i % 2) * (CARD_W + CARD_GAP);
  y = GRID_Y + (i / 2) * (CARD_H + CARD_GAP);
}

static void drawHeader() {
  gradPanel(&tft, HEADER_X, HEADER_Y, HEADER_W, HEADER_H, R_BUTTER, 5, 0);
  blitSprite(&tft, ITEM_FRAMES[ITEM_COIN], HEADER_X + 6, HEADER_Y + 1);
  char buf[12];
  snprintf(buf, sizeof(buf), "%u", player.ryo);
  drawText(&tft, FONT_CHUNK, buf, HEADER_X + 44, HEADER_Y + 9, 2, C_WHITE, C_OUT, TXT_OUTLINE);
  if (millis() < toastUntil && toast[0]) {
    drawText(&tft, FONT_SILK, toast, 228, HEADER_Y + 12, 2, R_RED.bot, C_WHITE, TXT_RIGHT | TXT_SHADOW);
    toastShown = true;
  } else {
    drawText(&tft, FONT_SILK, "SHOP", 228, HEADER_Y + 12, 2, C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
    toastShown = false;
  }
}

static void drawCard(int i) {
  static const Ramp ramps[SI_COUNT] = {R_PEACH, R_PINK, R_LAV, R_SKY, R_MINT, R_BUTTER};
  int x, y;
  cardRect(i, x, y);
  const ShopItemDef &d = SHOP_ITEMS[i];
  bool affordable = player.ryo >= d.price && player.items[i] < MAX_STACK;
  gradPanel(&tft, x, y, CARD_W, CARD_H, affordable ? ramps[i] : R_GREY, 5, 0);
  blitSprite(&tft, ITEM_FRAMES[d.sprite], x + 7, y + 6);
  drawText(&tft, FONT_SILK, d.name, x + 44, y + 11, 1, C_WHITE, C_OUT, TXT_OUTLINE);
  char buf[12];
  snprintf(buf, sizeof(buf), "OWN %d", player.items[i]);
  drawText(&tft, FONT_SILK, buf, x + 44, y + 25, 1, C_TEXT, C_WHITE, TXT_SHADOW);
  drawText(&tft, FONT_SILK, d.blurb, x + CARD_W / 2, y + 43, 1, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
  snprintf(buf, sizeof(buf), "%u", d.price);
  int w = 22 + textWidth(FONT_CHUNK, buf, 2);
  int px = x + (CARD_W - w) / 2;
  blitSpriteScaled(&tft, ITEM_FRAMES[ITEM_COIN], px, y + 51, 20, 20);
  drawText(&tft, FONT_CHUNK, buf, px + 24, y + 54, 2, C_WHITE, C_OUT, TXT_OUTLINE);
}

static void shopEnter() {
  drawGradientBg(R_PINK);
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 4);
  drawHeader();
  for (int i = 0; i < SI_COUNT; i++) drawCard(i);
}

static void say(const char *msg) {
  toast = msg;
  toastUntil = millis() + 1800;
  drawHeader();
}

static void shopTouch(int x, int y) {
  if (x < 52 && y < 52) {
    gotoScreen(SCR_MENU);
    return;
  }
  for (int i = 0; i < SI_COUNT; i++) {
    int cx, cy;
    cardRect(i, cx, cy);
    if (x < cx || x >= cx + CARD_W || y < cy || y >= cy + CARD_H) continue;
    const ShopItemDef &d = SHOP_ITEMS[i];
    if (player.items[i] >= MAX_STACK) {
      say("BAG FULL");
      hwBeep(300, 120);
    } else if (!playerSpendRyo(d.price)) {
      say("NO RYO!");
      hwBeep(300, 120);
    } else {
      player.items[i]++;
      playerSave();
      say("BOUGHT!");
      hwBeep(1318, 90);
    }
    // balances changed: refresh every card's affordability
    for (int k = 0; k < SI_COUNT; k++) drawCard(k);
    return;
  }
}

static void shopUpdate() {
  if (toastShown && millis() >= toastUntil) drawHeader();
}

const Screen SHOP_SCREEN = {shopEnter, shopUpdate, shopTouch};
