// Shared drawing helpers: banded-gradient panels, outlined pixel text, indexed sprites, button rows.
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "assets.h"
#include "font_data.h"

extern TFT_eSPI tft;
extern TFT_eSprite view;  // flicker-free animated area, recreated per screen

// ---- layout (portrait 240x320) ----
const int SCREEN_W = 240, SCREEN_H = 320;
const int NAV_H = 40, NAV_Y = 280;      // nav bar along the bottom
const int HUD_Y = 4, HUD_H = 34;        // stat bars along the top
const int BTN_Y = 214, BTN_H = 60;      // default position of a ButtonRow (just above the nav bar)
const int PAGE_Y = 276, PAGE_H = 40;    // BACK / NEXT page bar along the bottom of paged screens

// ---- palette (RGB565), set in gfxInit() ----
extern uint16_t C_OUT, C_TEXT, C_WHITE, C_CREAM, C_SHADOW, C_GOLD;
struct Ramp {
  uint16_t top, bot;  // a panel is shaded in stepped bands from top to bot
};
extern Ramp R_PINK, R_MINT, R_SKY, R_LAV, R_BUTTER, R_PEACH, R_CREAM, R_GREY, R_RED;

uint16_t mix565(uint16_t a, uint16_t b, int t256);

// Display tone correction (COLOR_GAMMA). Every palette colour goes through fx() when it is drawn;
// colours created in code use rgb(), which applies it once.
extern uint8_t LUT5[32], LUT6[64];
inline uint16_t fx(uint16_t c) { return (LUT5[c >> 11] << 11) | (LUT6[(c >> 5) & 63] << 5) | LUT5[c & 31]; }
uint16_t rgb(int r, int g, int b);  // t256 = 0 gives a, 256 gives b

void gfxInit();
bool gfxMakeView(int w, int h);

// Rounded panel with a dark outline and stepped gradient bands. lip = thickness of the darker
// 3D edge under it; pressed panels sit flush (no lip) and lose the highlight.
void gradPanel(TFT_eSPI *dst, int x, int y, int w, int h, Ramp r, int bands = 5, int lip = 0, bool pressed = false, int radius = 4);

// ---- sprites (palette indexed + run-length encoded, index 0 transparent) ----
void blitSprite(TFT_eSPI *dst, const Sprite &s, int x, int y);
void blitSpriteScaled(TFT_eSPI *dst, const Sprite &s, int x, int y, int dw, int dh, bool flipX = false);  // nearest neighbour
void blitSpriteHalf(TFT_eSPI *dst, const Sprite &s, int x, int y, bool flipX = false);  // smooth 2:1 reduction
void blitSilhouette(TFT_eSPI *dst, const Sprite &s, int x, int y, uint16_t color);
void blitSilhouetteScaled(TFT_eSPI *dst, const Sprite &s, int x, int y, int dw, int dh, uint16_t color);

// ---- text (bitmap fonts from tools/make_font.py) ----
enum { TXT_LEFT = 0, TXT_CENTER = 1, TXT_RIGHT = 2, TXT_OUTLINE = 4, TXT_SHADOW = 8 };
int textWidth(const PixelFont &f, const char *s, int scale);
void drawText(TFT_eSPI *dst, const PixelFont &f, const char *s, int x, int y, int scale, uint16_t fill, uint16_t edge, int flags = TXT_LEFT);

// ---- indexed backgrounds (240x320) ----
void drawBg(const uint8_t *idx, const uint16_t *pal);
// Redraws a rectangle of a background straight to the display (to erase something drawn over it)
void drawBgRegion(const uint8_t *idx, const uint16_t *pal, int x, int y, int w, int h);
void copyBgToView(const uint8_t *idx, const uint16_t *pal, int vx, int vy, int vw, int vh);

// A 5-segment stat bar with banded gradient segments
void drawSegBar(TFT_eSPI *dst, int x, int y, int filled, Ramp fillRamp, int segW = 8);

// Full-screen vertical gradient drawn as hard bands, for screens without a scene background
void drawGradientBg(Ramp r, int bands = 16);

enum { TAB_HOME, TAB_MENU, TAB_CLOCK, TAB_COUNT };
void drawNav(int activeTab);  // TAB_*; the bar sits at the bottom of the screen

// BACK / page counter / NEXT along the bottom, for screens whose content spans several pages
void pageNavDraw(int page, int pages);
int pageNavHit(int x, int y, int page, int pages);  // -1 = none, 0 = previous page, 1 = next page

struct BtnDef {
  const char *label;
  const Sprite *icon;  // nullptr = text-only button
  Ramp ramp;
};

// A row of colored touch buttons along the bottom of the screen with a short pressed animation
class ButtonRow {
 public:
  void set(const BtnDef *defs, int n);
  void setLabel(int i, const char *label);
  void draw();
  int hit(int x, int y) const;  // slot index or -1
  void press(int i);
  void update();  // releases the pressed look after a moment
  void setY(int y) { y_ = y; }  // top edge of the row (default BTN_Y)

 private:
  BtnDef defs_[6];
  int n_ = 0;
  int pressed_ = -1;
  int y_ = BTN_Y;
  uint32_t until_ = 0;
  void drawOne(int i, bool pressed);
};
