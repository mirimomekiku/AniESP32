#include "gfx.h"

#include "config.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite view = TFT_eSprite(&tft);

uint16_t C_OUT, C_TEXT, C_WHITE, C_CREAM, C_SHADOW, C_GOLD;
Ramp R_PINK, R_MINT, R_SKY, R_LAV, R_BUTTER, R_PEACH, R_CREAM, R_GREY, R_RED;

uint8_t LUT5[32], LUT6[64];

static void buildTone() {
  for (int i = 0; i < 32; i++) LUT5[i] = (uint8_t)roundf(powf(i / 31.0f, COLOR_GAMMA) * 31.0f);
  for (int i = 0; i < 64; i++) LUT6[i] = (uint8_t)roundf(powf(i / 63.0f, COLOR_GAMMA) * 63.0f);
}

// RGB888 -> RGB565 with rounding (tft.color565 truncates, which tints everything slightly green),
// then the tone curve.
uint16_t rgb(int r, int g, int b) {
  int r5 = (r * 31 + 127) / 255, g6 = (g * 63 + 127) / 255, b5 = (b * 31 + 127) / 255;
  return fx((r5 << 11) | (g6 << 5) | b5);
}

void gfxInit() {
  tft.init();
  tft.setRotation(0);  // portrait
  tft.setSwapBytes(true);
#ifndef CYD_NO_INVERT
  tft.invertDisplay(true);  // ILI9341 boards that show inverted colors (see cyd.md in the CYD repo)
#endif
  buildTone();

  C_OUT = rgb(58, 34, 82);  // dark plum outline, same as the sprite outlines
  C_TEXT = rgb(74, 44, 98);
  C_WHITE = rgb(255, 250, 240);
  C_CREAM = rgb(255, 244, 214);
  C_SHADOW = rgb(40, 22, 60);
  C_GOLD = rgb(255, 214, 90);

  R_PINK = {rgb(255, 188, 214), rgb(255, 118, 166)};
  R_MINT = {rgb(190, 246, 204), rgb(98, 204, 148)};
  R_SKY = {rgb(196, 232, 255), rgb(112, 178, 242)};
  R_LAV = {rgb(218, 200, 255), rgb(162, 134, 238)};
  R_BUTTER = {rgb(255, 247, 176), rgb(255, 206, 88)};
  R_PEACH = {rgb(255, 218, 184), rgb(255, 158, 114)};
  R_CREAM = {rgb(255, 252, 236), rgb(255, 232, 192)};
  R_RED = {rgb(255, 160, 160), rgb(228, 66, 84)};
  R_GREY = {rgb(234, 230, 240), rgb(190, 184, 206)};
}

bool gfxMakeView(int w, int h) {
  view.deleteSprite();
  view.setColorDepth(16);
  view.setSwapBytes(true);
  if (!view.createSprite(w, h)) {
    Serial.println("sprite alloc failed");
    return false;
  }
  return true;
}

// Blend two RGB565 colours. The result is snapped so red, green and blue step together: green has twice
// the levels of red and blue, so blending pale near-neutral colours would otherwise flip the tint
// (blue, grey, blue, grey...) from one gradient band to the next.
uint16_t mix565(uint16_t a, uint16_t b, int t) {
  int ar = (a >> 11) & 31, ag = ((a >> 5) & 63) >> 1, ab = a & 31;
  int br = (b >> 11) & 31, bg = ((b >> 5) & 63) >> 1, bb = b & 31;
  int r = (ar * (256 - t) + br * t + 128) >> 8;
  int g = (ag * (256 - t) + bg * t + 128) >> 8;
  int bl = (ab * (256 - t) + bb * t + 128) >> 8;
  return (r << 11) | (((g << 1) | (g >> 4)) << 5) | bl;
}

// ---- panels ----
// Pixel-art rounded corner: how many pixels row `row` (0 = outermost) is inset for radius r
static int cornerInset(int r, int row) {
  if (row >= r) return 0;
  float dy = r - row - 0.5f;
  return (int)roundf(r - sqrtf(max(0.0f, (float)(r * r) - dy * dy)));
}

static int rowInset(int h, int r, int row) {
  if (row < r) return cornerInset(r, row);
  if (row >= h - r) return cornerInset(r, h - 1 - row);
  return 0;
}

static void shape(TFT_eSPI *d, int x, int y, int w, int h, int r, uint16_t color) {
  for (int row = 0; row < h; row++) {
    int in = rowInset(h, r, row);
    d->fillRect(x + in, y + row, w - 2 * in, 1, color);
  }
}

void gradPanel(TFT_eSPI *d, int x, int y, int w, int h, Ramp ramp, int bands, int lip, bool pressed, int r) {
  if (pressed) {
    y += lip;  // sinks into its lip
    lip = 0;
  }
  if (lip > 0) shape(d, x, y + lip, w, h, r, C_SHADOW);  // 3D edge
  shape(d, x, y, w, h, r, C_OUT);

  // gradient interior, 2 px inside the outline, as hard-edged bands
  int ix = x + 2, iy = y + 2, iw = w - 4, ih = h - 4, ir = max(1, r - 2);
  Ramp rr = ramp;
  if (pressed) {
    rr.top = mix565(rr.top, C_OUT, 40);
    rr.bot = mix565(rr.bot, C_OUT, 40);
  }
  for (int row = 0; row < ih; row++) {
    int band = min(bands - 1, row * bands / ih);
    uint16_t c = mix565(rr.top, rr.bot, (band * 256 + 128) / bands);
    int in = rowInset(ih, ir, row);
    d->fillRect(ix + in, iy + row, iw - 2 * in, 1, c);
  }
  if (!pressed && ih > 8) {
    // glossy highlight along the top and a soft inner shade at the bottom
    int in = rowInset(ih, ir, 1) + 2;
    d->fillRect(ix + in + 1, iy + 1, iw - 2 * in - 2, 1, mix565(rr.top, C_WHITE, 150));
    d->fillRect(ix + 2, iy + ih - 2, iw - 4, 1, mix565(rr.bot, C_OUT, 60));
  }
}

// ---- sprites ----
void blitSprite(TFT_eSPI *d, const Sprite &s, int x, int y) {
  for (int j = 0; j < s.h; j++) {
    const uint8_t *row = s.px + j * s.w;
    int i = 0;
    while (i < s.w) {
      uint8_t v = row[i];
      if (!v) { i++; continue; }
      int k = i + 1;
      while (k < s.w && row[k] == v) k++;
      d->fillRect(x + i, y + j, k - i, 1, fx(s.pal[v]));
      i = k;
    }
  }
}

void blitSpriteScaled(TFT_eSPI *d, const Sprite &s, int x, int y, int dw, int dh, bool flipX) {
  for (int j = 0; j < s.h; j++) {
    int y0 = j * dh / s.h, y1 = (j + 1) * dh / s.h;
    if (y1 == y0) continue;
    const uint8_t *row = s.px + j * s.w;
    int i = 0;
    while (i < s.w) {
      uint8_t v = row[i];
      if (!v) { i++; continue; }
      int k = i + 1;
      while (k < s.w && row[k] == v) k++;
      int x0 = i * dw / s.w, x1 = k * dw / s.w;
      if (flipX) { int t = dw - x0; x0 = dw - x1; x1 = t; }
      if (x1 > x0) d->fillRect(x + x0, y + y0, x1 - x0, y1 - y0, fx(s.pal[v]));
      i = k;
    }
  }
}

void blitSpriteHalf(TFT_eSPI *d, const Sprite &s, int x, int y, bool flipX) {
  for (int j = 0; j + 1 < s.h; j += 2) {
    for (int i = 0; i + 1 < s.w; i += 2) {
      int n = 0, r = 0, g = 0, b = 0;
      for (int dj = 0; dj < 2; dj++)
        for (int di = 0; di < 2; di++) {
          uint8_t v = s.px[(j + dj) * s.w + i + di];
          if (!v) continue;
          uint16_t c = fx(s.pal[v]);
          r += (c >> 11) & 31;
          g += (c >> 5) & 63;
          b += c & 31;
          n++;
        }
      if (n >= 2) d->drawPixel(x + (flipX ? (s.w - 2 - i) / 2 : i / 2), y + j / 2, ((r / n) << 11) | ((g / n) << 5) | (b / n));
    }
  }
}

void blitSilhouette(TFT_eSPI *d, const Sprite &s, int x, int y, uint16_t color) {
  for (int j = 0; j < s.h; j++) {
    const uint8_t *row = s.px + j * s.w;
    int i = 0;
    while (i < s.w) {
      if (!row[i]) { i++; continue; }
      int k = i + 1;
      while (k < s.w && row[k]) k++;
      d->fillRect(x + i, y + j, k - i, 1, color);
      i = k;
    }
  }
}

void blitSilhouetteScaled(TFT_eSPI *d, const Sprite &s, int x, int y, int dw, int dh, uint16_t color) {
  for (int j = 0; j < s.h; j++) {
    int y0 = j * dh / s.h, y1 = (j + 1) * dh / s.h;
    if (y1 == y0) continue;
    const uint8_t *row = s.px + j * s.w;
    int i = 0;
    while (i < s.w) {
      if (!row[i]) { i++; continue; }
      int k = i + 1;
      while (k < s.w && row[k]) k++;
      int x0 = i * dw / s.w, x1 = k * dw / s.w;
      if (x1 > x0) d->fillRect(x + x0, y + y0, x1 - x0, y1 - y0, color);
      i = k;
    }
  }
}

void drawGradientBg(Ramp r, int bands) {
  for (int b = 0; b < bands; b++) {
    int y0 = b * SCREEN_H / bands, y1 = (b + 1) * SCREEN_H / bands;
    tft.fillRect(0, y0, SCREEN_W, y1 - y0, mix565(r.top, r.bot, (b * 256 + 128) / bands));
  }
}

// ---- text ----
static int glyphIndex(char c) { return (c >= 32 && c <= 126) ? c - 32 : '?' - 32; }

int textWidth(const PixelFont &f, const char *s, int scale) {
  int w = 0;
  for (; *s; s++) w += pgm_read_byte(&f.adv[glyphIndex(*s)]) * scale;
  return w;
}

static void glyphPass(TFT_eSPI *d, const PixelFont &f, const char *s, int x, int y, int scale, uint16_t color) {
  for (; *s; s++) {
    int g = glyphIndex(*s);
    const uint8_t *bits = f.bits + g * f.h;
    for (int row = 0; row < f.h; row++) {
      uint8_t b = pgm_read_byte(&bits[row]);
      int col = 0;
      while (col < 8) {
        if (!(b & (0x80 >> col))) { col++; continue; }
        int run = 1;
        while (col + run < 8 && (b & (0x80 >> (col + run)))) run++;
        d->fillRect(x + col * scale, y + row * scale, run * scale, scale, color);
        col += run;
      }
    }
    x += pgm_read_byte(&f.adv[g]) * scale;
  }
}

void drawText(TFT_eSPI *d, const PixelFont &f, const char *s, int x, int y, int scale, uint16_t fill, uint16_t edge, int flags) {
  int w = textWidth(f, s, scale);
  if (flags & TXT_CENTER) x -= w / 2;
  else if (flags & TXT_RIGHT) x -= w;
  if (flags & TXT_OUTLINE) {
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++)
        if (dx || dy) glyphPass(d, f, s, x + dx * scale, y + dy * scale, scale, edge);
  } else if (flags & TXT_SHADOW) {
    glyphPass(d, f, s, x + scale, y + scale, scale, edge);
  }
  glyphPass(d, f, s, x, y, scale, fill);
}

// ---- backgrounds ----
void drawBg(const uint8_t *idx, const uint16_t *pal) {
  static uint16_t buf[SCREEN_W * 8];
  for (int y0 = 0; y0 < SCREEN_H; y0 += 8) {
    for (int r = 0; r < 8; r++) {
      const uint8_t *row = idx + (y0 + r) * SCREEN_W;
      uint16_t *out = buf + r * SCREEN_W;
      for (int x = 0; x < SCREEN_W; x++) out[x] = fx(pal[row[x]]);
    }
    tft.pushImage(0, y0, SCREEN_W, 8, buf);
  }
}

void copyBgToView(const uint8_t *idx, const uint16_t *pal, int vx, int vy, int vw, int vh) {
  static uint16_t line[SCREEN_W];
  for (int r = 0; r < vh; r++) {
    const uint8_t *row = idx + (vy + r) * SCREEN_W + vx;
    for (int i = 0; i < vw; i++) line[i] = fx(pal[row[i]]);
    view.pushImage(0, r, vw, 1, line);
  }
}

// ---- stat bar ----
void drawSegBar(TFT_eSPI *d, int x, int y, int filled, Ramp fillRamp, int segW) {
  for (int s = 0; s < 5; s++) {
    int bx = x + s * segW;
    d->fillRect(bx, y, segW, 16, C_OUT);
    Ramp r = s < filled ? fillRamp : R_GREY;
    for (int row = 0; row < 12; row++) {
      int band = row * 3 / 12;
      d->fillRect(bx + 1, y + 2 + row, segW - 2, 1, mix565(r.top, r.bot, band * 128));
    }
    if (s < filled) d->fillRect(bx + 2, y + 2, segW - 4, 1, mix565(r.top, C_WHITE, 180));
  }
}

// ---- nav ----
// Little 3x3 grid of coloured tiles used as the MENU tab icon
static void drawMenuGlyph(int cx, int cy) {
  const uint16_t cols[9] = {R_PINK.bot, R_MINT.bot, R_SKY.bot, R_BUTTER.bot, C_WHITE, R_PEACH.bot, R_LAV.bot, R_PINK.top, R_MINT.top};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) {
      int x = cx - 15 + c * 10, y = cy - 15 + r * 10;
      tft.fillRect(x, y, 9, 9, C_OUT);
      tft.fillRect(x + 1, y + 1, 7, 7, cols[r * 3 + c]);
      tft.fillRect(x + 1, y + 1, 7, 2, mix565(cols[r * 3 + c], C_WHITE, 120));
    }
}

void drawNav(int activeTab) {
  const Ramp ramps[TAB_COUNT] = {R_PINK, R_LAV, R_MINT};
  const Sprite *icons[TAB_COUNT] = {&UI_FRAMES[UI_HOUSE], nullptr, &UI_FRAMES[UI_BAG]};
  tft.fillRect(0, NAV_Y, SCREEN_W, NAV_H, C_OUT);
  for (int i = 0; i < TAB_COUNT; i++) {
    bool active = activeTab == i;
    int x = i * 80 + 2, w = 76, h = NAV_H - 5;
    gradPanel(&tft, x, NAV_Y + 2, w, h, active ? ramps[i] : R_GREY, active ? 5 : 3, 3, active);
    int off = active ? 3 : 0;
    if (icons[i]) blitSprite(&tft, *icons[i], x + (w - 32) / 2, NAV_Y + 2 + (h - 32) / 2 + off);
    else drawMenuGlyph(x + w / 2, NAV_Y + 2 + h / 2 + off);
  }
}

// ---- ButtonRow ----
static const int BTN_MARGIN = 4, BTN_GAP = 3;

void ButtonRow::set(const BtnDef *defs, int n) {
  n_ = min(n, 6);
  for (int i = 0; i < n_; i++) defs_[i] = defs[i];
  pressed_ = -1;
}

void ButtonRow::setLabel(int i, const char *label) {
  if (i >= 0 && i < n_) defs_[i].label = label;
}

void ButtonRow::drawOne(int i, bool pressed) {
  int w = (SCREEN_W - 2 * BTN_MARGIN - BTN_GAP * (n_ - 1)) / n_;
  int x = BTN_MARGIN + i * (w + BTN_GAP);
  const BtnDef &d = defs_[i];
  tft.fillRect(x - 1, y_ - 1, w + 2, BTN_H + 2, C_OUT);  // clear to the strip colour
  gradPanel(&tft, x, y_, w, BTN_H - 4, d.ramp, 6, 4, pressed);
  int off = pressed ? 4 : 0;
  if (d.icon) {
    blitSprite(&tft, *d.icon, x + (w - 32) / 2, y_ + 4 + off);
    drawText(&tft, FONT_SILK, d.label, x + w / 2, y_ + 38 + off, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  } else {
    drawText(&tft, FONT_CHUNK, d.label, x + w / 2, y_ + 20 + off, 2, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  }
}

void ButtonRow::draw() {
  tft.fillRect(0, y_ - 2, SCREEN_W, BTN_H + 4, C_OUT);
  for (int i = 0; i < n_; i++) drawOne(i, false);
}

int ButtonRow::hit(int x, int y) const {
  if (y < y_ || y >= y_ + BTN_H) return -1;
  int w = (SCREEN_W - 2 * BTN_MARGIN - BTN_GAP * (n_ - 1)) / n_;
  for (int i = 0; i < n_; i++) {
    int bx = BTN_MARGIN + i * (w + BTN_GAP);
    if (x >= bx && x < bx + w) return i;
  }
  return -1;
}

void ButtonRow::press(int i) {
  pressed_ = i;
  until_ = millis() + 150;
  drawOne(i, true);
}

void ButtonRow::update() {
  if (pressed_ >= 0 && millis() > until_) {
    int i = pressed_;
    pressed_ = -1;
    drawOne(i, false);
  }
}
