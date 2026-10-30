#include "petview.h"

#include "hw.h"
#include "inventory.h"

static Anim anim = ANIM_NONE;
static uint32_t animUntil = 0, animTotal = 1;
static const Sprite *foodSprite = nullptr;
static const char *message = "";
static uint32_t messageUntil = 0;
static uint32_t lastCallBeep = 0;

void pvSay(const char *msg, uint32_t ms) {
  message = msg;
  messageUntil = millis() + ms;
}

const char *pvMessage() { return (millis() < messageUntil && message[0]) ? message : nullptr; }

void pvStartAnim(Anim a, uint32_t ms) {
  anim = a;
  animUntil = millis() + ms;
  animTotal = max<uint32_t>(ms, 1);
}

Anim pvAnim() {
  if (anim != ANIM_NONE && millis() > animUntil) anim = ANIM_NONE;
  return anim;
}

void pvSetFood(const Sprite *s) { foodSprite = s; }

// ---- pet events: sounds + speech ----
static void onPetEvent(PetEvent e) {
  switch (e) {
    case EV_HATCH: {
      static const int n[] = {660, 880, 1320};
      static const int d[] = {90, 90, 180};
      hwMelody(n, d, 3);
      pvSay("Hatched!", 3000);
      break;
    }
    case EV_EVOLVE:
      if (pet.stage == S_EGG) {
        pvSay("A new egg!", 3000);
      } else {
        static const int n[] = {523, 659, 784, 1047};
        static const int d[] = {100, 100, 100, 220};
        hwMelody(n, d, 4);
        pvSay("Evolved!", 3000);
      }
      break;
    case EV_POOP: pvSay("Poop!"); break;
    case EV_SICK: pvSay("Feels ill"); hwBeep(300, 250); break;
    case EV_DIE: {
      static const int n[] = {392, 330, 262};
      static const int d[] = {250, 250, 500};
      hwMelody(n, d, 3);
      pvSay("Goodbye...", 4000);
      break;
    }
    case EV_CARE_MISTAKE: hwBeep(220, 300); break;
    case EV_WAKE: pvSay("Morning!"); break;
    case EV_SLEEP: pvSay("Zzz..."); break;
    case EV_TANTRUM: pvSay("Hmph!", 3000); pvStartAnim(ANIM_GRUMPY, 3000); break;
    case EV_MISSION_DONE: {
      static const int n[] = {784, 988, 1175};
      static const int d[] = {90, 90, 200};
      hwMelody(n, d, 3);
      pvSay("Back home!", 3000);
      break;
    }
    case EV_CALL: break;  // handled by the repeating beep in pvUpdateCalls()
  }
}

void pvInit() { petEventHook = onPetEvent; }

// ---- mood and sprite choice ----
bool pvMoodSad() {
  Need n = petNeed();
  return pet.sick || pet.food < 1 || n == NEED_HUNGER || n == NEED_BORED || n == NEED_SICK || pet.food < 30 || pet.joy < 30;
}

bool pvMoodHappy() { return pet.food > 60 && pet.joy > 60 && pet.energy > 40 && !pet.sick; }

static int characterIndex() {
  switch (pet.stage) {
    case S_BABY: return CH_BABY;
    case S_CHILD: return CH_CHILD;
    case S_TEEN: return CH_TEEN;
    default: return CH_FIRST_ADULT + min<int>(pet.form, ADULT_COUNT - 1);
  }
}

const Sprite *pvPoseSprite(PoseId pose) { return &PET_SPRITES[characterIndex()][pose]; }

const Sprite *pvPickSprite(bool moving, bool sitting, int &bob, int &dx, float &sy) {
  uint32_t now = millis();
  float t = now / 1000.0f;
  Anim a = pvAnim();
  bob = 0;
  dx = 0;
  sy = 1.0f + 0.03f * sinf(t * 3.9f);  // breathing

  if (pet.stage == S_EGG) {
    sy = 1.0f;
    if (pet.warmth > 0) dx = ((now / 110) % 2 ? 1 : -1) * (1 + pet.warmth / 34) * 2;
    return &STAGE_FRAMES[pet.warmth >= 70 ? STAGE_CRACK : STAGE_EGG];
  }
  if (pet.stage == S_GHOST) {
    sy = 1.0f;
    bob = 6 + (int)(sinf(t * 2.0f) * 6);
    return &STAGE_FRAMES[STAGE_GHOST];
  }

  bool happyNow = a == ANIM_PLAY || (pvMoodHappy() && (now / 900) % 3 == 0);
  PoseId pose = POSE_IDLE_A;

  if (pet.sleeping) {
    pose = POSE_SLEEP;
    sy = 0.98f + 0.02f * sinf(t * 1.3f);  // slow breathing
  } else if (a == ANIM_EAT) {
    pose = ((now / 170) % 2) ? POSE_EAT_A : POSE_EAT_B;  // bite, chew, bite...
    sy = 1.0f + 0.03f * sinf(t * 14.0f);
  } else if (a == ANIM_CLEAN) {
    pose = ((now / 230) % 2) ? POSE_CLEAN_A : POSE_CLEAN_B;  // lathered, then shaking it off
    dx = (pose == POSE_CLEAN_B) ? (((now / 70) % 2) ? 2 : -2) : 0;
  } else if (a == ANIM_PLAY) {
    pose = POSE_HAPPY;
    bob = (int)fabsf(sinf(t * 8.0f) * 24);
    sy = 1.0f + 0.08f * cosf(t * 8.0f);
  } else if (a == ANIM_REFUSE || a == ANIM_GRUMPY) {
    dx = ((now / 90) % 2) ? 4 : -4;
  } else if (moving) {
    pose = ((now / 170) % 2) ? POSE_WALK_A : POSE_WALK_B;
    sy = 1.0f;
    bob = (int)(fabsf(sinf(now / 85.0f)) * 3);
  } else if (pet.sick || pet.food < 1 || (pvMoodSad() && !happyNow)) {
    pose = POSE_SAD;
    sy = 0.96f + 0.015f * sinf(t * 2.0f);
  } else if (happyNow) {
    pose = POSE_HAPPY;
    bob = (int)fabsf(sinf(t * 6.0f) * 8);
  } else if (sitting) {
    pose = POSE_SIT;
  } else {
    pose = (now % 3400 < 170) ? POSE_IDLE_B : POSE_IDLE_A;  // blink now and then
  }
  return pvPoseSprite(pose);
}

// ---- action feedback ----
void pvFeedback(ActionResult r, const char *okMsg, Anim okAnim, uint32_t ms) {
  switch (r) {
    case RES_OK:
      pvSay(okMsg);
      if (okAnim != ANIM_NONE) pvStartAnim(okAnim, ms);
      break;
    case RES_FULL: pvSay("Too full!"); pvStartAnim(ANIM_REFUSE, 900); break;
    case RES_ASLEEP: pvSay("Zzz.. shh!"); break;
    case RES_NOT_NEEDED: pvSay("Not needed"); break;
    case RES_TOO_TIRED: pvSay("Too tired"); break;
    case RES_FAINTED: pvSay("Feed me!"); break;
    case RES_EGG: pvSay("Still an egg"); break;
    case RES_NOT_ALIVE: pvSay("Tap the spirit"); break;
    case RES_NONE_LEFT: pvSay("Buy in SHOP"); break;
    case RES_AWAY: pvSay("On a mission"); break;
    case RES_TOO_YOUNG: pvSay("Too young"); break;
    case RES_UNWELL: pvSay("Not well"); break;
  }
}

// ---- drawing ----
void pvDrawEffects(TFT_eSPI *d, int cx, int floorY, int petH) {
  uint32_t now = millis();
  Anim a = pvAnim();
  float t = now / 1000.0f;
  if (a == ANIM_EAT && foodSprite) {
    float p = 1.0f - (float)(animUntil - now) / animTotal;
    int s = max(24, petH / 2);
    if (p < 0.6f) blitSpriteScaled(d, *foodSprite, cx - s / 2, floorY - petH - s + (int)(p / 0.6f * petH * 0.7f), s, s);
  } else if (a == ANIM_POOF) {
    int s = petH + 16;
    blitSpriteScaled(d, ITEM_FRAMES[ITEM_SMOKE], cx - s / 2, floorY - petH / 2 - s / 2, s, s);
  } else if (a == ANIM_CLEAN) {
    // soap bubbles drifting up around the pet
    uint16_t fill = mix565(C_WHITE, R_SKY.top, 90);
    for (int i = 0; i < 9; i++) {
      float ph = fmodf(t * 0.7f + i * 0.113f, 1.0f);
      int r = 3 + (i % 3) * 2 + (int)(petH / 40);
      int bx = cx + (int)(sinf(t * 2.2f + i * 1.7f) * petH * 0.45f) + (i - 4) * (petH / 10);
      int by = floorY - 6 - (int)(ph * (petH + 24));
      d->fillCircle(bx, by, r, fill);
      d->drawCircle(bx, by, r, R_SKY.bot);
      d->fillRect(bx - r / 2, by - r / 2, max(1, r / 3), max(1, r / 3), C_WHITE);  // little shine
    }
  } else if (a == ANIM_PLAY || (pvMoodHappy() && !pet.sleeping && pet.stage != S_EGG && pet.stage != S_GHOST)) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(t * 1.1f + i * 0.33f, 1.0f);
      const Sprite &deco = DECOR_FRAMES[a == ANIM_PLAY ? DECOR_HEART : (i % 2 ? DECOR_SPARKLE : DECOR_PLUS)];
      int span = max(60, petH);
      blitSprite(d, deco, cx - span / 2 - 12 + i * (span / 2), floorY - petH / 3 - (int)(ph * petH * 0.8f));
    }
  }
}

void pvDrawBubble(TFT_eSPI *d, int vw, int anchorX, int anchorTopY) {
  const char *m = pvMessage();
  if (!m) return;
  const int H = 30, TAIL = 6;
  int w = textWidth(FONT_SILK, m, 2) + 20;
  int bx = constrain(anchorX - w / 2, 2, vw - w - 2);
  int by = max(2, anchorTopY - H - TAIL - 2);
  gradPanel(d, bx, by, w, H, R_CREAM, 3, 0);
  int tx = constrain(anchorX, bx + 10, bx + w - 10);  // the tail points at the pet
  d->fillRect(tx - 5, by + H, 10, 4, C_OUT);
  d->fillRect(tx - 3, by + H - 2, 6, 5, C_CREAM);
  d->fillRect(tx - 2, by + H + 4, 4, 2, C_OUT);
  drawText(d, FONT_SILK, m, bx + w / 2, by + 10, 2, C_TEXT, C_WHITE, TXT_CENTER);
}

// ---- need label ----
static int shownNeed = -2;  // -2 = nothing known on screen

static const char *needText(Need n) {
  switch (n) {
    case NEED_SICK: return "FEELS ILL";
    case NEED_HUNGER: return "HUNGRY!";
    case NEED_POOP: return "TOO DIRTY!";
    case NEED_TANTRUM: return "GRUMPY!";
    case NEED_BORED: return "BORED!";
    case NEED_SLEEPY: return "SLEEPY!";
    default: return "";
  }
}

void pvNeedLabelDraw(const uint8_t *bg, const uint16_t *pal, int xRight, int y, bool force) {
  Need n = (pet.sleeping || mission.active) ? NEED_NONE : petNeed();
  if (!force && (int)n == shownNeed) return;
  const int H = 24, MAXW = 150;
  if (!force && shownNeed > 0) drawBgRegion(bg, pal, xRight - MAXW, y, MAXW + 2, H + 4);  // erase the old label
  shownNeed = n;
  if (n == NEED_NONE) return;
  const char *t = needText(n);
  int w = textWidth(FONT_SILK, t, 2) + 18;
  Ramp r = (n == NEED_SICK || n == NEED_HUNGER) ? R_RED : R_BUTTER;
  gradPanel(&tft, xRight - w, y, w, H, r, 4, 0);
  drawText(&tft, FONT_SILK, t, xRight - w / 2, y + 7, 2, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);
}

// ---- stat bars ----
static int shownSeg[2][3] = {{-1, -1, -1}, {-1, -1, -1}};

void pvHudDraw(bool compact, int x0, bool force) {
  float vals[3] = {pet.food, pet.joy, pet.energy};
  const Sprite *icons[3] = {&UI_FRAMES[UI_APPLE], &UI_FRAMES[UI_HEART], &UI_FRAMES[UI_BOLT]};
  const Ramp ramps[3] = {R_PEACH, R_PINK, R_BUTTER};
  for (int i = 0; i < 3; i++) {
    int segs = (int)ceilf(vals[i] / 20.0f);  // 0..5
    int &shown = shownSeg[compact][i];
    if (!force && segs == shown) continue;
    shown = segs;
    Ramp fill = segs <= 1 ? R_RED : ramps[i];
    if (!compact) {
      int x = x0 + i * 79;
      gradPanel(&tft, x, HUD_Y, 77, HUD_H, R_CREAM, 4, 0);
      // icon and bar both centred on the panel's vertical middle
      blitSprite(&tft, *icons[i], x + 3, HUD_Y + (HUD_H - 32) / 2);
      drawSegBar(&tft, x + 36, HUD_Y + (HUD_H - 16) / 2, segs, fill);
    } else {
      int x = x0 + i * 62;
      gradPanel(&tft, x, 8, 60, 28, R_CREAM, 4, 0);
      blitSpriteScaled(&tft, *icons[i], x + 3, 8 + (28 - 22) / 2, 22, 22);
      drawSegBar(&tft, x + 24, 8 + (28 - 16) / 2, segs, fill, 7);
    }
  }
}

void pvUpdateCalls() {
  bool calling = petNeed() != NEED_NONE && !pet.sleeping;
  hwAttention(calling);
  if (calling && millis() - lastCallBeep > 45000) {
    lastCallBeep = millis();
    hwBeep(1175, 120);
  }
}
