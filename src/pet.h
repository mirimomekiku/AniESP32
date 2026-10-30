// Game logic: no drawing, no hardware. All time is in "game seconds" (see TIME_SCALE in config.h).
#pragma once
#include <Arduino.h>

enum Stage : uint8_t { S_EGG, S_BABY, S_CHILD, S_TEEN, S_ADULT, S_GHOST };
enum Need : uint8_t { NEED_NONE, NEED_SICK, NEED_HUNGER, NEED_POOP, NEED_TANTRUM, NEED_BORED, NEED_SLEEPY };
enum PetEvent : uint8_t {
  EV_HATCH, EV_EVOLVE, EV_POOP, EV_SICK, EV_DIE, EV_CALL, EV_CARE_MISTAKE, EV_WAKE, EV_SLEEP, EV_TANTRUM, EV_MISSION_DONE
};
enum ActionResult : uint8_t { RES_OK, RES_FULL, RES_ASLEEP, RES_NOT_NEEDED, RES_TOO_TIRED, RES_FAINTED, RES_NOT_ALIVE, RES_EGG, RES_NONE_LEFT, RES_AWAY, RES_TOO_YOUNG, RES_UNWELL };

struct Pet {
  uint8_t version;
  uint8_t stage;
  uint8_t form;  // adult form index (ADULT_* in assets.h), valid when stage == S_ADULT
  uint8_t sleeping;
  uint8_t sick;
  uint8_t poop;
  uint8_t careMistakes;
  uint8_t needKind;
  uint8_t needPenalized;
  uint8_t tantrum;
  uint8_t stayUp;   // woken at night on purpose: don't put it back to sleep until morning
  uint16_t warmth;  // egg only, 0..100
  float food, joy, energy;
  float discipline, bond;
  uint32_t stageAge;     // game seconds spent in the current stage
  uint32_t poopTimer;    // game seconds until the next poop
  uint32_t needSince;    // stageAge when the current need started
  uint32_t sickFor;
  uint32_t poopNeglect;
  uint32_t starving;
  uint32_t tantrumIn;
  uint32_t tantrumFor;
  uint32_t lifeSeconds;  // total age of this pet
};

// Missions: the pet leaves home for a while and comes back with rewards
struct MissionDef {
  const char *name;
  uint32_t seconds;  // game seconds
  int ryo, joy, discipline, energy, food;
};
const int MISSION_COUNT = 3;
extern const MissionDef MISSIONS[MISSION_COUNT];
struct MissionState {
  uint8_t active;    // 0 = at home, 1 = away
  uint8_t kind;      // index into MISSIONS
  uint32_t left;     // game seconds until it returns
};
extern MissionState mission;

extern Pet pet;
extern bool petDark;   // set by main from the light sensor: lights out
extern bool petNight;  // set by main from the clock: the pet sleeps at night
extern void (*petEventHook)(PetEvent);  // set by the UI to react to events

void petNewEgg();
bool petLoad();
void petSave();
void petTick();                       // advance one game second
void petCatchUp(uint32_t seconds);    // simulate time the device spent switched off

ActionResult petEat(float food, float joy);  // eat something; refuses when full
ActionResult petRestoreEnergy(float amount);
ActionResult petTrain(float discipline);
ActionResult petPlay();
ActionResult petClean();
ActionResult petHeal();
ActionResult petToggleSleep();
void petAlarmWake();  // the alarm clock rang: wake the pet up
ActionResult petStartMission(int kind);
ActionResult petCanPlayGame();  // can the pet take part in a mini-game right now?
void petApplyGame(int joy, int discipline, int energyCost, int ryo, float bond);
void petTap();  // tap the pet itself: warm the egg, scold a tantrum, or just pet it

Need petNeed();
const char *stageName();
const char *formName();
const char *adultName(int form);
