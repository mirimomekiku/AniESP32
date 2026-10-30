// Shop screen: spend ryo on food, medicine and training scrolls.
// Two categories (FOOD, ITEMS), four large cards per page, BACK / NEXT along the bottom.
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "screen.h"

static const int CARD_W = 114, CARD_H = 94, CARD_GAP = 4;
static const int GRID_X = 4, GRID_Y = 78;
static const int PER_PAGE = 4;
static const int HEADER_Y = 8, HEADER_X = 54, HEADER_W = 182, HEADER_H = 34;
static const int CHIP_Y = 48, CHIP_H = 24;

static int category = 0;  // 0 = food, 1 = items
static int pageNo = 0;
static const char *toast = "";
static uint32_t toastUntil = 0;
static bool toastShown = false;

static int listSize() { return category == 0 ? 12 : 3; }
static ShopItem listItem(int i) { return category == 0 ? SHOP_FOODS[i] : SHOP_OTHERS[i]; }
static int pageCount() { return (listSize() + PER_PAGE - 1) / PER_PAGE; }

static void cardRect(int slot, int &x, int &y) {
  x = GRID_X + (slot % 2) * (CARD_W + CARD_GAP);
  y = GRID_Y + (slot / 2) * (CARD_H + CARD_GAP);
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

static void drawChips() {
  const char *names[2] = {"FOOD", "ITEMS"};
  for (int i = 0; i < 2; i++) {
    int x = 4 + i * 118, w = 114;
    bool on = category == i;
    gradPanel(&tft, x, CHIP_Y, w, CHIP_H, on ? R_BUTTER : R_GREY, 4, on ? 0 : 2);
    drawTextC(&tft, FONT_SILK, names[i], x + w / 2, CHIP_Y + CHIP_H / 2 - (on ? 0 : 1), 2, on ? C_WHITE : C_TEXT, on ? C_OUT : C_WHITE,
              on ? TXT_OUTLINE : TXT_SHADOW);
  }
}

static void drawCard(int slot) {
  static const Ramp ramps[6] = {R_PEACH, R_PINK, R_LAV, R_SKY, R_MINT, R_BUTTER};
  int x, y;
  cardRect(slot, x, y);
  int li = pageNo * PER_PAGE + slot;
  if (li >= listSize()) return;  // empty slot, the background shows through
  ShopItem id = listItem(li);
  const ShopItemDef &d = SHOP_ITEMS[id];
  bool affordable = player.ryo >= d.price && player.items[id] < MAX_STACK;
  gradPanel(&tft, x, y, CARD_W, CARD_H, affordable ? ramps[li % 6] : R_GREY, 5, 0);

  blitSprite(&tft, itemSprite(id), x + (CARD_W - 32) / 2, y + 6);
  if (player.items[id] > 0) {  // how many you already own
    char own[8];
    snprintf(own, sizeof(own), "x%d", player.items[id]);
    int w = textWidth(FONT_SILK, own, 1) + 10;
    gradPanel(&tft, x + CARD_W - w - 5, y + 5, w, 15, R_CREAM, 3, 0, false, 3);
    drawText(&tft, FONT_SILK, own, x + CARD_W - 5 - w / 2, y + 8, 1, C_TEXT, C_WHITE, TXT_CENTER);
  }
  // a modest title (the chunky 8 px font at 1x), then the effects as icon + number
  drawText(&tft, FONT_CHUNK, d.name, x + CARD_W / 2, y + 41, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);

  ItemEffect fx[4];
  int ne = itemEffects(id, fx);
  char val[8][8];
  int total = 0;
  for (int i = 0; i < ne; i++) {
    if (fx[i].kind == EF_CURE) snprintf(val[i], sizeof(val[i]), "CURE");
    else snprintf(val[i], sizeof(val[i]), "+%d", fx[i].value);
    total += 16 + 1 + textWidth(FONT_SILK, val[i], 1) + (i ? 5 : 0);
  }
  int ex = x + (CARD_W - total) / 2;
  for (int i = 0; i < ne; i++) {
    if (i) ex += 5;
    blitSpriteHalf(&tft, effectIcon(fx[i].kind), ex, y + 55);  // 32 px icon averaged down to 16
    drawTextC(&tft, FONT_SILK, val[i], ex + 16 + 1 + textWidth(FONT_SILK, val[i], 1) / 2, y + 55 + 8, 1, C_TEXT, C_WHITE, TXT_SHADOW);
    ex += 16 + 1 + textWidth(FONT_SILK, val[i], 1);
  }

  char price[8];
  snprintf(price, sizeof(price), "%u", d.price);
  int w = 20 + 4 + textWidth(FONT_CHUNK, price, 2);
  int px = x + (CARD_W - w) / 2, py = y + CARD_H - 24;
  blitSpriteScaled(&tft, ITEM_FRAMES[ITEM_COIN], px, py, 20, 20);
  drawText(&tft, FONT_CHUNK, price, px + 24, py + 3, 2, C_WHITE, C_OUT, TXT_OUTLINE);
}

static void drawCards() {
  for (int i = 0; i < PER_PAGE; i++) drawCard(i);
}

static void drawAll() {
  drawGradientBg(R_PINK);
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 4);
  drawHeader();
  drawChips();
  drawCards();
  pageNavDraw(pageNo, pageCount());
}

static void shopEnter() {
  category = 0;
  pageNo = 0;
  drawAll();
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
  // BACK / NEXT
  int nav = pageNavHit(x, y, pageNo, pageCount());
  if (nav >= 0) {
    pageNo += nav == 0 ? -1 : 1;
    drawAll();
    return;
  }
  // category chips
  if (y >= CHIP_Y && y < CHIP_Y + CHIP_H) {
    int c = x < 120 ? 0 : 1;
    if (c != category) {
      category = c;
      pageNo = 0;
      drawAll();
    }
    return;
  }
  for (int slot = 0; slot < PER_PAGE; slot++) {
    int cx, cy;
    cardRect(slot, cx, cy);
    int li = pageNo * PER_PAGE + slot;
    if (li >= listSize() || x < cx || x >= cx + CARD_W || y < cy || y >= cy + CARD_H) continue;
    ShopItem id = listItem(li);
    const ShopItemDef &d = SHOP_ITEMS[id];
    if (player.items[id] >= MAX_STACK) {
      say("BAG FULL");
      hwBeep(300, 120);
    } else if (!playerSpendRyo(d.price)) {
      say("NO RYO!");
      hwBeep(300, 120);
    } else {
      player.items[id]++;
      playerSave();
      say("BOUGHT!");
      hwBeep(1318, 90);
    }
    drawCards();  // balances changed: refresh affordability and owned counts
    return;
  }
}

static void shopUpdate() {
  if (toastShown && millis() >= toastUntil) drawHeader();
}

const Screen SHOP_SCREEN = {shopEnter, shopUpdate, shopTouch};
