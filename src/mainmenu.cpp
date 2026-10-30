// Main menu: a 3x3 grid of round badge buttons.
#include "gfx.h"
#include "screen.h"

struct Entry {
  const Sprite *badge;
  int screen;  // where it leads
};

static const int N = 9;
static const Entry ENTRIES[N] = {
    {&MENU_FRAMES[MENU_FEED], SCR_FEED},   {&MENU_FRAMES[MENU_PLAY], SCR_PLAY}, {&MENU_FRAMES[MENU_CLEAN], SCR_CLEAN},
    {&MENU_FRAMES[MENU_BAG], SCR_BAG},     {&MENU_FRAMES[MENU_SLEEP], SCR_SLEEP}, {&MENU_FRAMES[MENU_SHOP], SCR_SHOP},
    {&MENU_FRAMES[MENU_DEX], SCR_ALBUM},   {&MENU2_FRAMES[MENU2_STATS], SCR_STATS}, {&MENU2_FRAMES[MENU2_HELP], SCR_HELP},
};

// A 3x3 grid of badges, the whole block centred above the nav bar
static const int ROW_COUNTS[3] = {3, 3, 3};
static const int GAP_X = 6, GAP_Y = 18;
static const int TOP = (NAV_Y - (3 * MENU_SIZE + 2 * GAP_Y)) / 2;

static void badgePos(int i, int &x, int &y) {
  int row = 0, first = 0;
  while (row < 2 && i >= first + ROW_COUNTS[row]) first += ROW_COUNTS[row++];
  int n = ROW_COUNTS[row], col = i - first;
  int rowW = n * MENU_SIZE + (n - 1) * GAP_X;
  x = (SCREEN_W - rowW) / 2 + col * (MENU_SIZE + GAP_X);
  y = TOP + row * (MENU_SIZE + GAP_Y);
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
    blitSprite(&tft, *ENTRIES[i].badge, x, y);
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
    if (x >= bx && x < bx + MENU_SIZE && y >= by && y < by + MENU_SIZE) {
      gotoScreen(ENTRIES[i].screen);
      return;
    }
  }
}

const Screen MENU_SCREEN = {menuEnter, menuUpdate, menuTouch};
