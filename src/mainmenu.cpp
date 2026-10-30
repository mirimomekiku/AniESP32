// Main menu: a 3x3 grid of round badge buttons.
#include "gfx.h"
#include "screen.h"

struct Entry {
  const char *label;
  int sprite;  // MENU_* in assets.h
  int screen;  // where it leads
};

static const int N = 7;
static const Entry ENTRIES[N] = {
    {"FEED", MENU_FEED, SCR_FEED},   {"PLAY", MENU_PLAY, SCR_PLAY},   {"CLEAN", MENU_CLEAN, SCR_CLEAN},
    {"BAG", MENU_BAG, SCR_BAG},      {"SLEEP", MENU_SLEEP, SCR_SLEEP},
    {"SHOP", MENU_SHOP, SCR_SHOP},   {"DEX", MENU_DEX, SCR_ALBUM},
};

// Rows of 3, 2 and 2 badges, each row centred
static const int ROW_COUNTS[3] = {3, 2, 2};
static const int GAP_X = 6, ROW_PITCH = 90, TOP = 10;

static void badgePos(int i, int &x, int &y) {
  int row = 0, first = 0;
  while (row < 2 && i >= first + ROW_COUNTS[row]) first += ROW_COUNTS[row++];
  int n = ROW_COUNTS[row], col = i - first;
  int rowW = n * MENU_SIZE + (n - 1) * GAP_X;
  x = (SCREEN_W - rowW) / 2 + col * (MENU_SIZE + GAP_X);
  y = TOP + row * ROW_PITCH;
}

static void menuEnter() {
  drawGradientBg({rgb(255, 251, 255), rgb(240, 214, 255)}, 10);
  // soft lavender frame, in the spirit of a handheld's screen bezel
  tft.drawRect(0, 0, SCREEN_W, NAV_Y, R_LAV.bot);
  tft.drawRect(1, 1, SCREEN_W - 2, NAV_Y - 2, R_LAV.top);
  blitSprite(&tft, DECOR_FRAMES[DECOR_SPARKLE], 214, 2);
  blitSprite(&tft, DECOR_FRAMES[DECOR_PLUS], 4, NAV_Y - 28);

  for (int i = 0; i < N; i++) {
    int x, y;
    badgePos(i, x, y);
    blitSprite(&tft, MENU_FRAMES[ENTRIES[i].sprite], x, y);
    drawText(&tft, FONT_SILK, ENTRIES[i].label, x + MENU_SIZE / 2, y + MENU_SIZE + 3, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  }
  drawNav(TAB_MENU);
}

static void menuUpdate() {}

static void menuTouch(int x, int y) {
  if (x >= 206 && y < 10) {  // hidden corner: colour test screen
    gotoScreen(SCR_DIAG);
    return;
  }
  for (int i = 0; i < N; i++) {
    int bx, by;
    badgePos(i, bx, by);
    if (x >= bx && x < bx + MENU_SIZE && y >= by && y < by + MENU_SIZE + 12) {
      gotoScreen(ENTRIES[i].screen);
      return;
    }
  }
}

const Screen MENU_SCREEN = {menuEnter, menuUpdate, menuTouch};
