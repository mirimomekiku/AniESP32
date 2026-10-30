// A screen owns the whole display below the nav tabs. main.cpp switches between them.
#pragma once
#include <Arduino.h>

struct Screen {
  void (*enter)();             // full redraw, create the view sprite
  void (*update)();            // called every loop
  void (*touch)(int x, int y); // finger just went down at (x, y), below the nav tabs
};

extern const Screen VILLAGE_SCREEN;
extern const Screen MENU_SCREEN;
extern const Screen CLOCK_SCREEN;
extern const Screen SHOP_SCREEN;
extern const Screen ALBUM_SCREEN;
extern const Screen FEED_SCREEN;
extern const Screen PLAY_SCREEN;
extern const Screen CLEAN_SCREEN;
extern const Screen BAG_SCREEN;
extern const Screen SLEEP_SCREEN;
extern const Screen SHURIKEN_SCREEN;
extern const Screen SHELL_SCREEN;
extern const Screen DIAG_SCREEN;

// The first three are the nav-bar screens, in the same order as the TAB_* constants in gfx.h.
// Everything after SCR_CLOCK is a full-screen page without the nav bar (it has its own back icon).
enum ScreenId {
  SCR_VILLAGE, SCR_MENU, SCR_CLOCK,
  SCR_SHOP, SCR_ALBUM,
  SCR_FEED, SCR_PLAY, SCR_CLEAN, SCR_BAG, SCR_SLEEP,
  SCR_SHURIKEN, SCR_SHELL, SCR_DIAG, SCR_COUNT
};
void gotoScreen(int id);  // fades to black, switches screen, fades back in

void villageInit();  // wires the pet event hook
