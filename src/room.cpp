// Room screens (feed, play, clean, bag, sleep): the pet roams a room in small size.
// Back icon top-left, stat bars along the top, action icons bottom-right.
#include "config.h"
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "itemmenu.h"
#include "pet.h"
#include "petview.h"
#include "screen.h"

enum Action { AC_FOOD, AC_PLAYMENU, AC_MISSIONS, AC_CLEAN, AC_HEAL, AC_BAGMENU, AC_SLEEP };
struct RoomIcon {
  int sprite;  // ROOMUI_*
  int action;
};
struct RoomDef {
  const uint8_t *bg;
  const uint16_t *pal;
  int nIcons;
  RoomIcon icons[2];
};

static const RoomDef ROOMS[5] = {
    {BG_ROOM_FEED, BG_ROOM_FEED_PAL, 1, {{ROOMUI_FOOD, AC_FOOD}}},
    {BG_ROOM_PLAY, BG_ROOM_PLAY_PAL, 2, {{ROOMUI_PLAY, AC_PLAYMENU}, {ROOMUI_MISSION, AC_MISSIONS}}},
    {BG_ROOM_CLEAN, BG_ROOM_CLEAN_PAL, 2, {{ROOMUI_CLEAN, AC_CLEAN}, {ROOMUI_HEAL, AC_HEAL}}},
    {BG_ROOM_BAG, BG_ROOM_BAG_PAL, 1, {{ROOMUI_BAG, AC_BAGMENU}}},
    {BG_ROOM_SLEEP, BG_ROOM_SLEEP_PAL, 1, {{ROOMUI_SLEEP, AC_SLEEP}}},
};

static const int VX = 0, VY = 136, VW = 240, VH = 134;  // roaming window: screen y 136..270
static const int PET = 48;                              // drawn size of the small pet
static const int MIN_X = 30, MAX_X = 210, MIN_Y = 74, MAX_Y = 124;  // where the feet may go (view coords)
static const int ICON_Y = SCREEN_H - ROOMUI_SIZE - 4;

static const RoomDef *room = &ROOMS[0];
static int roomIdx = 0;
static float px = 120, py = 100, tx = 120, ty = 100;
static bool faceRight = true, moving = false;
static uint32_t pauseUntil = 0, lastFrame = 0, lastMove = 0;

static int iconX(int i) { return SCREEN_W - 4 - (room->nIcons - i) * (ROOMUI_SIZE + 4) + 4; }

static void drawStatic() {
  drawBg(room->bg, room->pal);
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 2);
  pvHudDraw(true, 52, true);
  for (int i = 0; i < room->nIcons; i++) blitSprite(&tft, ROOMUI_FRAMES[room->icons[i].sprite], iconX(i), ICON_Y);
}

static void enterRoom(int idx) {
  roomIdx = idx;
  room = &ROOMS[idx];
  gfxMakeView(VW, VH);
  px = tx = 120;
  py = ty = 100;
  pauseUntil = millis() + 800;
  drawStatic();
}

static void pickTarget() {
  tx = MIN_X + esp_random() % (MAX_X - MIN_X);
  ty = MIN_Y + esp_random() % (MAX_Y - MIN_Y);
  moving = true;
}

static bool canRoam() {
  return pet.stage != S_EGG && pet.stage != S_GHOST && !pet.sleeping && !mission.active && pvAnim() == ANIM_NONE;
}

static void roamStep(float dt) {
  if (!canRoam()) {
    moving = false;
    return;
  }
  uint32_t now = millis();
  if (!moving) {
    if (now > pauseUntil) pickTarget();
    return;
  }
  float dx = tx - px, dy = ty - py;
  float dist = sqrtf(dx * dx + dy * dy);
  float step = 28.0f * dt;
  if (dist <= step) {
    px = tx;
    py = ty;
    moving = false;
    pauseUntil = now + 900 + esp_random() % 2500;
    return;
  }
  px += dx / dist * step;
  py += dy / dist * step;
  if (fabsf(dx) > 2) faceRight = dx > 0;
}

static void drawView() {
  uint32_t now = millis();
  copyBgToView(room->bg, room->pal, VX, VY, VW, VH);

  // the clean room shows every poop pile lying around
  if (roomIdx == 2) {
    static const int pos[4][2] = {{20, 70}, {176, 78}, {82, 98}, {138, 62}};
    for (int i = 0; i < pet.poop && i < 4; i++) blitSpriteScaled(&view, ITEM_FRAMES[ITEM_POOP], pos[i][0], pos[i][1], 36, 36);
  }

  if (mission.active) {
    drawText(&view, FONT_SILK, "ON A MISSION", VW / 2, 70, 2, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
    char buf[16];
    uint32_t left = mission.left / TIME_SCALE;
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(left / 60), (unsigned long)(left % 60));
    drawText(&view, FONT_CHUNK, buf, VW / 2, 90, 3, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
    view.pushSprite(VX, VY);
    return;
  }

  int bob, dx;
  float sy;
  const Sprite *sprite = pvPickSprite(bob, dx, sy);
  int walkBob = moving ? (int)(fabsf(sinf(now / 90.0f)) * 3) : 0;
  int cx = (int)px + dx / 2;
  int feet = (int)py;

  uint16_t ground = view.readPixel(constrain(cx, 0, VW - 1), constrain(feet + 2, 0, VH - 1));
  view.fillEllipse(cx, feet, 18 - bob / 6, 4, mix565(ground, C_OUT, 70));
  blitSpriteHalf(&view, *sprite, cx - PET / 2, feet - PET + 2 - walkBob - bob / 2, !faceRight);

  if (pet.sleeping && pet.stage != S_EGG && pet.stage != S_GHOST) {
    int off = (int)(fmodf(now / 1000.0f, 3.0f) * 6);
    drawText(&view, FONT_CHUNK, "Z", cx + 14, feet - PET - off, 2, R_PINK.bot, C_WHITE, TXT_OUTLINE);
  }
  pvDrawEffects(&view, cx, feet, PET);
  pvDrawNeedBubble(&view, VW);
  pvDrawBubble(&view, VW, 0);
  view.pushSprite(VX, VY);
}

// ---- input ----
static bool blocked() {
  if (pet.stage == S_EGG) { pvFeedback(RES_EGG, "", ANIM_NONE, 0); return true; }
  if (pet.stage == S_GHOST) { pvFeedback(RES_NOT_ALIVE, "", ANIM_NONE, 0); return true; }
  if (mission.active) { pvFeedback(RES_AWAY, "", ANIM_NONE, 0); return true; }
  return false;
}

static void act(int a) {
  switch (a) {
    case AC_FOOD: if (!blocked()) imOpen(IM_FEED, drawStatic); break;
    case AC_PLAYMENU: if (!blocked()) imOpen(IM_PLAY, drawStatic); break;
    case AC_MISSIONS: if (!blocked()) imOpen(IM_MISSION, drawStatic); break;
    case AC_BAGMENU: if (!blocked()) imOpen(IM_BAG, drawStatic); break;
    case AC_CLEAN: pvFeedback(petClean(), "Fresh!", ANIM_CLEAN, 700); break;
    case AC_HEAL:
      pvSetFood(&ITEM_FRAMES[ITEM_MEDICINE]);
      pvFeedback(useItem(SI_MEDICINE), "Better!", ANIM_CLEAN, 700);
      playerSave();
      break;
    case AC_SLEEP: {
      ActionResult r = petToggleSleep();
      pvFeedback(r, pet.sleeping ? "Good night" : "Morning!", ANIM_NONE, 0);
      break;
    }
  }
  petSave();
}

static void roomTouch(int x, int y) {
  if (imActive()) {
    imTouch(x, y);
    return;
  }
  if (x < 52 && y < 50) {
    gotoScreen(SCR_MENU);
    return;
  }
  for (int i = 0; i < room->nIcons; i++) {
    int ix = iconX(i);
    if (x >= ix && x < ix + ROOMUI_SIZE && y >= ICON_Y && y < ICON_Y + ROOMUI_SIZE) {
      act(room->icons[i].action);
      return;
    }
  }
  // tapping the pet pets it, warms an egg or scolds a tantrum
  if (y >= VY && y < VY + VH && abs(x - (int)px) < 36 && abs(y - VY - (int)py + PET / 2) < 40) {
    petTap();
    if (pet.stage == S_EGG) pvSay(pet.warmth >= 70 ? "Cracking!" : "Warm...", 900);
    else if (pet.stage != S_GHOST) pvSay(pet.tantrum ? "Hmph!" : "Hehe!", 900);
    petSave();
  }
}

static void roomUpdate() {
  uint32_t now = millis();
  if (imActive()) return;  // the list is static; pause the room

  pvHudDraw(true, 52, false);
  pvUpdateCalls();

  if (now - lastFrame >= 50) {  // ~20 fps
    float dt = (now - lastMove) / 1000.0f;
    lastMove = now;
    lastFrame = now;
    roamStep(min(dt, 0.2f));
    drawView();
  }
}

static void feedEnter() { enterRoom(0); }
static void playEnter() { enterRoom(1); }
static void cleanEnter() { enterRoom(2); }
static void bagEnter() { enterRoom(3); }
static void sleepEnter() { enterRoom(4); }

const Screen FEED_SCREEN = {feedEnter, roomUpdate, roomTouch};
const Screen PLAY_SCREEN = {playEnter, roomUpdate, roomTouch};
const Screen CLEAN_SCREEN = {cleanEnter, roomUpdate, roomTouch};
const Screen BAG_SCREEN = {bagEnter, roomUpdate, roomTouch};
const Screen SLEEP_SCREEN = {sleepEnter, roomUpdate, roomTouch};
