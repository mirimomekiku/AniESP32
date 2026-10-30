// How the pet is presented: sprite choice, animations, speech, stat bars and event reactions.
// Shared by the home screen and the room screens.
#pragma once
#include <Arduino.h>

#include "gfx.h"
#include "pet.h"

enum Anim { ANIM_NONE, ANIM_EAT, ANIM_PLAY, ANIM_REFUSE, ANIM_CLEAN, ANIM_GRUMPY };

void pvInit();  // wires the pet event hook (sounds + speech)

void pvSay(const char *msg, uint32_t ms = 2000);
const char *pvMessage();  // nullptr when nothing is being said
void pvStartAnim(Anim a, uint32_t ms);
Anim pvAnim();
void pvSetFood(const Sprite *s);  // what the pet is eating

bool pvMoodSad();
bool pvMoodHappy();
// Chooses the frame and idle motion: bob (px up), dx (px sideways), sy (vertical stretch factor)
const Sprite *pvPickSprite(int &bob, int &dx, float &sy);

// Reaction text and animation for the result of an action
void pvFeedback(ActionResult r, const char *okMsg, Anim okAnim, uint32_t ms);

// Drawing helpers (dst is the screen or a view sprite). cx = pet centre x, floorY = pet feet, petH = drawn height
void pvDrawEffects(TFT_eSPI *dst, int cx, int floorY, int petH);
void pvDrawBubble(TFT_eSPI *dst, int vw, int y = 4);
void pvDrawNeedBubble(TFT_eSPI *dst, int vw);

// Stat bars (food, joy, energy). compact = small version used on room screens (x0 = left edge).
void pvHudDraw(bool compact, int x0, bool force);

// Attention call: LED blink and a repeating beep while a need is unanswered
void pvUpdateCalls();
