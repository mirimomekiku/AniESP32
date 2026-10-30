// Mini-games played in the dojo: shuriken timing and the shadow-clone shell game.
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "pet.h"
#include "petview.h"
#include "screen.h"

static const int GV_Y = 70, GV_W = 240, GV_H = 150;  // animated view: screen y 70..220
static ButtonRow buttons;
static uint32_t lastFrame = 0;

// ---- shared helpers ----
static const Sprite *petFrame(bool happy) { return pvPoseSprite(happy ? POSE_HAPPY : POSE_IDLE_A); }

static void drawHeader(const char *left, const char *right) {
  gradPanel(&tft, 4, 8, 232, 36, R_BUTTER, 5, 0);
  drawText(&tft, FONT_CHUNK, left, 14, 18, 2, C_WHITE, C_OUT, TXT_OUTLINE);
  drawText(&tft, FONT_SILK, right, 226, 20, 2, C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
}

static void setButtons(const char *a, const char *b) {
  BtnDef defs[2] = {{a, nullptr, R_PINK}, {b, nullptr, R_SKY}};
  buttons.setY(254);
  buttons.set(defs, b ? 2 : 1);
  buttons.draw();
}

static void summary(const char *title, int score, int maxScore, int ryo) {
  gradPanel(&view, 24, 12, 192, 126, R_CREAM, 6, 0);
  drawText(&view, FONT_SILK, title, 120, 24, 2, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
  char buf[24];
  snprintf(buf, sizeof(buf), "%d / %d", score, maxScore);
  drawText(&view, FONT_CHUNK, buf, 120, 54, 3, R_PINK.bot, C_OUT, TXT_CENTER | TXT_OUTLINE);
  blitSprite(&view, ITEM_FRAMES[ITEM_COIN], 70, 92);
  snprintf(buf, sizeof(buf), "+%d", ryo);
  drawText(&view, FONT_CHUNK, buf, 108, 100, 2, C_WHITE, C_OUT, TXT_OUTLINE);
}

// =====================================================================
// Shuriken timing: let the shuriken fly when the target will be in front of you
// =====================================================================
enum SState { SS_READY, SS_FLY, SS_RESULT, SS_END };
static SState ss = SS_READY;
static int throwNo = 0, score = 0, lastPts = 0, ssRyo = 0;
static float tx = 40, tdir = 1, tspeed = 70;
static uint32_t ssSince = 0, ssLast = 0;
static const int THROWS = 5, TARGET_Y = 34, PET_X = 120;
static const uint32_t FLIGHT_MS = 280;

static void ssHeader() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d/%d", min(throwNo + 1, THROWS), THROWS);
  char sc[16];
  snprintf(sc, sizeof(sc), "SCORE %d", score);
  drawHeader(buf, sc);
}

static void ssReset() {
  ss = SS_READY;
  throwNo = score = lastPts = 0;
  tx = 40;
  tdir = 1;
  tspeed = 70;
  ssLast = millis();
}

static void shurikenEnter() {
  gfxMakeView(GV_W, GV_H);
  drawBg(BG_DOJO, BG_DOJO_PAL);
  ssReset();
  ssHeader();
  setButtons("THROW!", "QUIT");
}

static void ssThrow() {
  if (ss != SS_READY) return;
  ss = SS_FLY;
  ssSince = millis();
  hwBeep(900, 40);
}

static void ssFinish() {
  ss = SS_END;
  ssRyo = score * 2;
  petApplyGame(score, score / 3, 15, ssRyo, score >= 12 ? 1 : 0);
  setButtons("AGAIN", "BACK");
  static const int n[] = {523, 659, 784};
  static const int d[] = {90, 90, 160};
  if (score >= 6) hwMelody(n, d, 3);
}

static void ssUpdate() {
  uint32_t now = millis();
  buttons.update();
  if (now - lastFrame < 33) return;
  lastFrame = now;
  float dt = (now - ssLast) / 1000.0f;
  ssLast = now;

  // the target keeps sliding left and right, even while the shuriken is flying
  if (ss != SS_END) {
    tx += tdir * tspeed * dt;
    if (tx > 206) { tx = 206; tdir = -1; }
    if (tx < 34) { tx = 34; tdir = 1; }
  }

  float flight = ss == SS_FLY ? min(1.0f, (now - ssSince) / (float)FLIGHT_MS) : 0;
  if (ss == SS_FLY && flight >= 1.0f) {
    float off = fabsf(tx - PET_X);
    lastPts = off < 8 ? 3 : (off < 18 ? 2 : (off < 30 ? 1 : 0));
    score += lastPts;
    ss = SS_RESULT;
    ssSince = now;
    hwBeep(lastPts ? 600 + lastPts * 250 : 200, 80);
    ssHeader();
  } else if (ss == SS_RESULT && now - ssSince > 800) {
    throwNo++;
    tspeed += 28;
    if (throwNo >= THROWS) ssFinish();
    else { ss = SS_READY; ssHeader(); }
  }

  copyBgToView(BG_DOJO, BG_DOJO_PAL, 0, GV_Y, GV_W, GV_H);

  // target: post, then red/white rings
  int cx = (int)tx;
  view.fillRect(cx - 3, TARGET_Y + 18, 6, 34, C_OUT);
  view.fillRect(cx - 1, TARGET_Y + 18, 2, 34, rgb(168, 110, 80));
  view.fillCircle(cx, TARGET_Y, 25, C_OUT);
  view.fillCircle(cx, TARGET_Y, 23, R_RED.bot);
  view.fillCircle(cx, TARGET_Y, 16, C_WHITE);
  view.fillCircle(cx, TARGET_Y, 10, R_RED.bot);
  view.fillCircle(cx, TARGET_Y, 4, R_BUTTER.bot);
  view.drawCircle(cx, TARGET_Y, 16, C_OUT);
  view.drawCircle(cx, TARGET_Y, 10, C_OUT);

  // the pet at the bottom, hopping with joy after a good hit
  bool happy = ss == SS_RESULT && lastPts >= 2;
  int hop = happy ? (int)fabsf(sinf(now / 90.0f) * 8) : 0;
  blitSpriteScaled(&view, *petFrame(happy), PET_X - 36, GV_H - 72 - hop, 72, 72);

  if (ss == SS_FLY) {
    int y0 = GV_H - 80, y1 = TARGET_Y;
    int y = y0 + (int)((y1 - y0) * flight);
    int sz = 40 - (int)(16 * flight);  // shrinks as it flies away from us
    blitSpriteScaled(&view, ITEM_FRAMES[ITEM_SHURIKEN], PET_X - sz / 2, y - sz / 2, sz, sz);
  } else if (ss == SS_RESULT) {
    blitSpriteScaled(&view, ITEM_FRAMES[ITEM_SHURIKEN], PET_X - 11, TARGET_Y - 11, 22, 22);
    const char *txt[4] = {"MISS", "GRAZE +1", "HIT +2", "BULLSEYE +3"};
    int w = textWidth(FONT_SILK, txt[lastPts], 2) + 20;
    gradPanel(&view, PET_X - w / 2, 76, w, 28, lastPts ? R_BUTTER : R_GREY, 4, 0);
    drawText(&view, FONT_SILK, txt[lastPts], PET_X, 85, 2, C_TEXT, C_WHITE, TXT_CENTER | TXT_SHADOW);
  } else if (ss == SS_READY && throwNo == 0 && now % 1200 < 800) {
    drawText(&view, FONT_SILK, "TAP WHEN THE TARGET", PET_X, 90, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
    drawText(&view, FONT_SILK, "IS IN FRONT OF YOU", PET_X, 101, 1, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  }

  if (ss == SS_END) summary(score >= 12 ? "NINJA MASTER" : (score >= 6 ? "NICE THROWS" : "KEEP TRAINING"), score, THROWS * 3, ssRyo);
  view.pushSprite(0, GV_Y);
}

static void ssTouch(int x, int y) {
  int i = buttons.hit(x, y);
  if (i >= 0) {
    buttons.press(i);
    if (ss == SS_END) {
      if (i == 0) {
        if (petCanPlayGame() == RES_OK) { ssReset(); ssHeader(); setButtons("THROW!", "QUIT"); }
        else gotoScreen(SCR_VILLAGE);
      } else gotoScreen(SCR_VILLAGE);
    } else if (i == 0) ssThrow();
    else gotoScreen(SCR_VILLAGE);
    return;
  }
  if (y >= GV_Y && y < GV_Y + GV_H) ssThrow();
}

const Screen SHURIKEN_SCREEN = {shurikenEnter, ssUpdate, ssTouch};

// =====================================================================
// Shell game: follow the real pet through a shuffle of smoke clouds
// =====================================================================
enum GState { GS_SHOW, GS_SHUFFLE, GS_CHOOSE, GS_REVEAL, GS_END };
static GState gs = GS_SHOW;
static const int ROUNDS = 3, SLOT_X[3] = {40, 120, 200}, CLOUD_Y = 62;
static int round_ = 0, wins = 0, petCloud = 1, picked = -1, gsRyo = 0;
static int posOf[3] = {0, 1, 2};  // slot index each cloud currently sits in
static int swapsLeft = 0, swapA = 0, swapB = 1;
static uint32_t gsSince = 0, swapMs = 320;

static void gsHeader() {
  char buf[16];
  snprintf(buf, sizeof(buf), "ROUND %d/%d", min(round_ + 1, ROUNDS), ROUNDS);
  char sc[16];
  snprintf(sc, sizeof(sc), "WINS %d", wins);
  drawHeader(buf, sc);
}

static void gsStartRound() {
  gs = GS_SHOW;
  gsSince = millis();
  petCloud = esp_random() % 3;
  for (int i = 0; i < 3; i++) posOf[i] = i;
  swapsLeft = 5 + round_ * 2;
  swapMs = 320 - round_ * 70;
  picked = -1;
  gsHeader();
}

static void gsNewGame() {
  round_ = wins = 0;
  gsStartRound();
}

static void shellEnter() {
  gfxMakeView(GV_W, GV_H);
  drawBg(BG_DOJO, BG_DOJO_PAL);
  gsNewGame();
  setButtons("QUIT", nullptr);
}

static void pickSwap() {
  swapA = esp_random() % 3;
  do swapB = esp_random() % 3; while (swapB == swapA);
  gsSince = millis();
}

static void gsFinish() {
  gs = GS_END;
  gsRyo = wins * 6;
  petApplyGame(wins * 5, 0, 8, gsRyo, wins == ROUNDS ? 1 : 0);
  setButtons("AGAIN", "BACK");
}

// x position of a cloud, interpolated while it is being swapped
static float cloudX(int c, float t) {
  float x = SLOT_X[posOf[c]];
  if (gs == GS_SHUFFLE && (c == swapA || c == swapB)) {
    int other = c == swapA ? swapB : swapA;
    x += (SLOT_X[posOf[other]] - x) * t;
  }
  return x;
}

static void shellUpdate() {
  uint32_t now = millis();
  buttons.update();
  if (now - lastFrame < 33) return;
  lastFrame = now;

  float t = 0;
  if (gs == GS_SHOW && now - gsSince > 1300) { gs = GS_SHUFFLE; pickSwap(); }
  if (gs == GS_SHUFFLE) {
    t = min(1.0f, (now - gsSince) / (float)swapMs);
    if (t >= 1.0f) {
      int tmp = posOf[swapA];
      posOf[swapA] = posOf[swapB];
      posOf[swapB] = tmp;
      if (--swapsLeft <= 0) { gs = GS_CHOOSE; t = 0; }
      else { pickSwap(); t = 0; }
    }
  }
  if (gs == GS_REVEAL && now - gsSince > 1100) {
    round_++;
    if (round_ >= ROUNDS) gsFinish();
    else gsStartRound();
  }

  copyBgToView(BG_DOJO, BG_DOJO_PAL, 0, GV_Y, GV_W, GV_H);

  // the pet sits on the ground under its cloud
  bool revealed = gs == GS_SHOW || gs == GS_REVEAL || gs == GS_END;
  if (gs == GS_SHOW || gs == GS_REVEAL) {
    bool happy = gs == GS_REVEAL && picked == petCloud;
    int px = (int)cloudX(petCloud, 0);
    blitSpriteScaled(&view, *petFrame(happy), px - 34, CLOUD_Y + 14, 68, 68);
  }
  for (int c = 0; c < 3; c++) {
    float x = cloudX(c, t);
    float lift = 0;
    if (gs == GS_SHUFFLE && (c == swapA || c == swapB)) lift = sinf(t * 3.14159f) * (c == swapA ? -14 : 14);
    // while the real pet is shown or revealed, its cloud floats up out of the way
    if (revealed && c == petCloud) lift -= 36;
    blitSpriteScaled(&view, ITEM_FRAMES[ITEM_SMOKE], (int)x - 32, CLOUD_Y + (int)lift - 14, 64, 64);
  }
  const char *msg = gs == GS_SHOW ? "WATCH CLOSELY!" : (gs == GS_SHUFFLE ? "..." : (gs == GS_CHOOSE ? "WHERE IS IT?" : (picked == petCloud ? "FOUND IT!" : "MISSED!")));
  if (gs != GS_END) drawText(&view, FONT_SILK, msg, 120, 6, 2, gs == GS_REVEAL ? (picked == petCloud ? R_BUTTER.top : R_RED.top) : C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
  if (gs == GS_END) summary(wins == ROUNDS ? "PERFECT" : (wins ? "NICE EYES" : "TRY AGAIN"), wins, ROUNDS, gsRyo);
  view.pushSprite(0, GV_Y);
}

static void shellTouch(int x, int y) {
  int i = buttons.hit(x, y);
  if (i >= 0) {
    buttons.press(i);
    if (gs == GS_END && i == 0) {
      if (petCanPlayGame() == RES_OK) { gsNewGame(); setButtons("QUIT", nullptr); }
      else gotoScreen(SCR_VILLAGE);
    } else if (gs == GS_END || i == 0) {
      gotoScreen(SCR_VILLAGE);
    }
    return;
  }
  if (gs != GS_CHOOSE || y < GV_Y || y >= GV_Y + GV_H) return;
  for (int c = 0; c < 3; c++) {
    if (abs(x - SLOT_X[posOf[c]]) < 30) {
      picked = c;
      if (c == petCloud) { wins++; hwBeep(1175, 100); } else hwBeep(220, 160);
      gs = GS_REVEAL;
      gsSince = millis();
      gsHeader();
      return;
    }
  }
}

const Screen SHELL_SCREEN = {shellEnter, shellUpdate, shellTouch};
