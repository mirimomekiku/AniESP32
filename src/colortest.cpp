// Hidden colour test screen (tap the top-right corner of the main menu).
// Draws raw colour bars, channel ramps and the UI pastels, with and without the tone curve,
// to see exactly how the panel renders them. Tap anywhere to leave.
#include "gfx.h"
#include "screen.h"

static void label(const char *s, int cx, int y) { drawText(&tft, FONT_SILK, s, cx, y, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE); }

static void diagEnter() {
  tft.fillScreen(tft.color565(128, 128, 128));
  drawText(&tft, FONT_SILK, "COLOR TEST (RAW)", 120, 4, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);

  // pure colours, no tone curve
  const struct { const char *n; uint8_t r, g, b; } bars[8] = {{"R", 255, 0, 0}, {"G", 0, 255, 0}, {"B", 0, 0, 255}, {"C", 0, 255, 255},
                                                              {"M", 255, 0, 255}, {"Y", 255, 255, 0}, {"W", 255, 255, 255}, {"K", 0, 0, 0}};
  for (int i = 0; i < 8; i++) {
    tft.fillRect(i * 30, 16, 30, 28, tft.color565(bars[i].r, bars[i].g, bars[i].b));
    label(bars[i].n, i * 30 + 15, 48);
  }

  // 16-step ramps: grey, red, green, blue
  const char *names[4] = {"GREY", "RED", "GREEN", "BLUE"};
  for (int row = 0; row < 4; row++) {
    for (int i = 0; i < 16; i++) {
      int v = i * 17;
      uint16_t c = row == 0 ? tft.color565(v, v, v) : row == 1 ? tft.color565(v, 0, 0) : row == 2 ? tft.color565(0, v, 0) : tft.color565(0, 0, v);
      tft.fillRect(i * 15, 64 + row * 22, 15, 20, c);
    }
    label(names[row], 120, 70 + row * 22);
  }

  // the UI pastels: raw (top) and through the tone curve (bottom)
  const struct { const char *n; uint8_t r, g, b; } pastel[6] = {{"PINK", 255, 188, 214}, {"PEACH", 255, 218, 184}, {"MINT", 190, 246, 204},
                                                                {"SKY", 196, 232, 255},  {"LAV", 218, 200, 255},   {"BUTTER", 255, 247, 176}};
  drawText(&tft, FONT_SILK, "PASTELS: RAW", 120, 158, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  for (int i = 0; i < 6; i++) {
    tft.fillRect(i * 40, 170, 40, 34, tft.color565(pastel[i].r, pastel[i].g, pastel[i].b));
    label(pastel[i].n, i * 40 + 20, 184);
  }
  drawText(&tft, FONT_SILK, "PASTELS: WITH TONE CURVE", 120, 210, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  for (int i = 0; i < 6; i++) {
    tft.fillRect(i * 40, 222, 40, 34, rgb(pastel[i].r, pastel[i].g, pastel[i].b));
    label(pastel[i].n, i * 40 + 20, 236);
  }
  // a banded gradient like the UI panels
  for (int b = 0; b < 12; b++) tft.fillRect(b * 20, 262, 20, 20, mix565(R_PINK.top, R_PINK.bot, b * 256 / 11));
  for (int b = 0; b < 12; b++) tft.fillRect(b * 20, 284, 20, 20, mix565(R_SKY.top, R_SKY.bot, b * 256 / 11));
  label("TAP ANYWHERE TO EXIT", 120, 308);
}

static void diagUpdate() {}
static void diagTouch(int, int) { gotoScreen(SCR_MENU); }

const Screen DIAG_SCREEN = {diagEnter, diagUpdate, diagTouch};
