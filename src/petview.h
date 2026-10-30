// How the pet is presented: sprite choice, animations, speech, stat bars and event reactions.
// Shared by the home screen and the room screens.
#pragma once
#include <Arduino.h>

#include "gfx.h"
#include "pet.h"

// ANIM_CLEAN = being washed (soap bubbles); ANIM_POOF = a puff of smoke after using a potion, scroll or medicine
enum Anim { ANIM_NONE, ANIM_EAT, ANIM_PLAY, ANIM_REFUSE, ANIM_CLEAN, ANIM_GRUMPY, ANIM_POOF };

void pvInit();  // wires the pet event hook (sounds + speech)

void pvSay(const char *msg, uint32_t ms = 2000);
const char *pvMessage();  // nullptr when nothing is being said
void pvStartAnim(Anim a, uint32_t ms);
Anim pvAnim();
void pvSetFood(const Sprite *s);  // what the pet is eating

bool pvMoodSad();
bool pvMoodHappy();
// Chooses the animation frame for the current state (sleeping, eating, being cleaned, walking, sitting, happy, sad,
// idle with blinks) plus the idle motion: bob (px up), dx (px sideways), sy (vertical stretch factor).
// moving / sitting are supplied by the screen (a roaming pet walks, a resting one sits).
const Sprite *pvPickSprite(bool moving, bool sitting, int &bob, int &dx, float &sy);
// A given pose of the current character (growing stages and adults), for screens that draw it themselves
const Sprite *pvPoseSprite(PoseId pose);

// Reaction text and animation for the result of an action
void pvFeedback(ActionResult r, const char *okMsg, Anim okAnim, uint32_t ms);

// Drawing helpers (dst is the screen or a view sprite). cx = pet centre x, floorY = pet feet, petH = drawn height
void pvDrawEffects(TFT_eSPI *dst, int cx, int floorY, int petH);
// Small speech bubble just above the pet's head. anchorX = pet centre x, headTopY = the top of the visible head
// (use spriteTopRow), both in dst coordinates. The bubble leans a little toward the middle of the screen (right
// when the pet is on the left half and vice versa), stays inside [0, vw) and its tail points at the pet.
void pvDrawBubble(TFT_eSPI *dst, int vw, int anchorX, int headTopY);

// The pet's condition as up to two text labels, right-aligned at xRight with the top at y: "HUNGRY!", "UNHAPPY!",
// "SLEEPING"... The background is restored when they change. force = the screen was just redrawn.
void pvNeedLabelDraw(const uint8_t *bg, const uint16_t *pal, int xRight, int y, bool force);

// Stat bars (food, joy, energy). compact = small version used on room screens (x0 = left edge).
void pvHudDraw(bool compact, int x0, bool force);

// Attention call: LED blink and a repeating beep while a need is unanswered
void pvUpdateCalls();
