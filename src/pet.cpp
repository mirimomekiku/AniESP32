#include "pet.h"

#include <Preferences.h>

#include "album.h"
#include "assets.h"
#include "config.h"
#include "inventory.h"

Pet pet;
MissionState mission;

const MissionDef MISSIONS[MISSION_COUNT] = {
    // name      secs  ryo joy disc energy food
    {"ERRAND", 300, 8, 0, 0, 5, 3},
    {"SCOUT", 900, 20, 10, 0, 15, 8},
    {"TRAINING", 1800, 15, 0, 10, 25, 12},
};
bool petNight = false;
bool petDark = false;
static uint32_t darkFor = 0, lightFor = 0;
static bool darkSlept = false;  // put to bed by the lights going out (not saved)
void (*petEventHook)(PetEvent) = nullptr;

static const uint8_t SAVE_VERSION = 2;

// ---- timings (game seconds) ----
static const uint32_t EGG_TIME = 120;
static const uint32_t BABY_TIME = 30UL * 60;
static const uint32_t CHILD_TIME = 6UL * 3600;
static const uint32_t TEEN_TIME = 24UL * 3600;
static const uint32_t NEED_GRACE = 15UL * 60;   // ignored this long = care mistake
static const uint32_t SICK_DEATH = 90UL * 60;   // untreated this long = ghost

static void emit(PetEvent e) {
  if (petEventHook) petEventHook(e);
}

static float clampf(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

static uint32_t randomRange(uint32_t lo, uint32_t hi) { return lo + (esp_random() % (hi - lo + 1)); }

static void scheduleNextPoop() { pet.poopTimer = randomRange(40UL * 60, 90UL * 60); }
static void scheduleNextTantrum() { pet.tantrumIn = randomRange(2UL * 3600, 4UL * 3600); }

void petNewEgg() {
  memset(&pet, 0, sizeof(pet));
  memset(&mission, 0, sizeof(mission));
  pet.version = SAVE_VERSION;
  pet.stage = S_EGG;
  pet.food = pet.joy = pet.energy = 80;
  pet.discipline = 40;
  pet.bond = 30;
  scheduleNextPoop();
  scheduleNextTantrum();
}

// ---- persistence ----
bool petLoad() {
  Preferences prefs;
  prefs.begin("tama", true);
  size_t n = prefs.getBytesLength("pet");
  bool ok = false;
  if (n == sizeof(Pet)) {
    Pet tmp;
    prefs.getBytes("pet", &tmp, sizeof(Pet));
    if (tmp.version == SAVE_VERSION) {
      pet = tmp;
      ok = true;
    }
  }
  if (ok && prefs.getBytesLength("mission") == sizeof(MissionState)) prefs.getBytes("mission", &mission, sizeof(MissionState));
  else memset(&mission, 0, sizeof(mission));
  prefs.end();
  if (!ok) petNewEgg();
  return ok;
}

void petSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putBytes("pet", &pet, sizeof(Pet));
  prefs.putBytes("mission", &mission, sizeof(MissionState));
  prefs.end();
}

// ---- evolution: first matching row wins ----
struct EvoRule {
  uint8_t maxCM;
  uint8_t minD, maxD;
  uint8_t minB;
  uint8_t minE;
  uint8_t minJ;
  uint8_t form;
};

static const EvoRule EVO[] = {
    // maxCM minD maxD minB minE minJ  form
    {1, 70, 100, 70, 0, 0, ADULT_FOX},
    {1, 70, 100, 0, 0, 0, ADULT_TOAD},
    {1, 0, 69, 60, 0, 70, ADULT_RABBIT},
    {1, 0, 69, 0, 0, 0, ADULT_WEASEL},
    {3, 70, 100, 50, 0, 0, ADULT_SHEEP},
    {3, 70, 100, 0, 0, 0, ADULT_TOAD},
    {3, 0, 69, 0, 0, 70, ADULT_AXOLOTL},
    {3, 0, 69, 0, 0, 0, ADULT_TANUKI},
    {5, 0, 100, 0, 70, 0, ADULT_PENGUIN},
    {5, 0, 100, 0, 50, 0, ADULT_HAWK},
    {5, 0, 100, 0, 0, 0, ADULT_SLUG},
    {255, 50, 100, 0, 0, 0, ADULT_CAT},
    {255, 0, 49, 0, 0, 0, ADULT_SNAKE},
};

static uint8_t pickForm() {
  for (const EvoRule &r : EVO) {
    if (pet.careMistakes <= r.maxCM && pet.discipline >= r.minD && pet.discipline <= r.maxD && pet.bond >= r.minB &&
        pet.energy >= r.minE && pet.joy >= r.minJ)
      return r.form;
  }
  return ADULT_SNAKE;
}

static void advanceStage(Stage next) {
  pet.stage = next;
  pet.stageAge = 0;
  if (next == S_ADULT) {
    pet.form = pickForm();
    albumUnlock(pet.form);
  }
  playerAddRyo(next == S_BABY ? 10 : 30);
  if (next == S_BABY) emit(EV_HATCH);
  else emit(EV_EVOLVE);
}

// ---- needs ----
Need petNeed() {
  if (pet.stage == S_EGG || pet.stage == S_GHOST || mission.active) return NEED_NONE;
  if (pet.sick) return NEED_SICK;
  if (pet.food < 30) return NEED_HUNGER;
  if (pet.poop >= 2) return NEED_POOP;
  if (pet.tantrum) return NEED_TANTRUM;
  if (pet.joy < 30) return NEED_BORED;
  if (!pet.sleeping && pet.energy < 20) return NEED_SLEEPY;
  return NEED_NONE;
}

static void die() {
  pet.stage = S_GHOST;
  pet.stageAge = 0;
  pet.sick = 0;
  pet.sleeping = 0;
  pet.poop = 0;
  emit(EV_DIE);
}

static void finishMission() {
  const MissionDef &m = MISSIONS[mission.kind];
  mission.active = 0;
  playerAddRyo(m.ryo);
  pet.joy = clampf(pet.joy + m.joy);
  pet.discipline = clampf(pet.discipline + m.discipline);
  pet.energy = clampf(pet.energy - m.energy);
  pet.food = clampf(pet.food - m.food);
  pet.bond = clampf(pet.bond + 1);
  emit(EV_MISSION_DONE);
}

void petTick() {
  pet.lifeSeconds++;
  pet.stageAge++;

  if (pet.stage == S_GHOST) return;

  if (pet.stage == S_EGG) {
    if (pet.warmth >= 100 || pet.stageAge >= EGG_TIME) advanceStage(S_BABY);
    return;
  }

  // vitals
  if (petDark) { darkFor++; lightFor = 0; } else { lightFor++; darkFor = 0; }
  if (!petNight && !petDark) pet.stayUp = 0;
  if (pet.sleeping) {
    pet.energy = clampf(pet.energy + 0.03f);
    pet.food = clampf(pet.food - 0.002f);
    // sleeps through the night; by day it wakes when rested
    bool rested = pet.energy >= 100 && !petNight && !petDark;
    bool lightsOn = darkSlept && !petDark && !petNight && lightFor >= 5;
    if (rested || lightsOn) {
      pet.sleeping = 0;
      darkSlept = false;
      emit(EV_WAKE);
    }
  } else {
    pet.food = clampf(pet.food - 0.010f);
    pet.joy = clampf(pet.joy - (pet.food < 25 || pet.sick ? 0.020f : 0.007f));
    pet.energy = clampf(pet.energy - 0.004f);
    bool lightsOut = petDark && darkFor >= 20 && !pet.stayUp && !mission.active;
    if (pet.energy <= 0 || (petNight && !pet.stayUp && pet.energy < 70) || lightsOut) {
      pet.sleeping = 1;
      darkSlept = lightsOut;
      emit(EV_SLEEP);
    }
    // errands: a little pocket money while it is awake
    if (pet.stage >= S_CHILD && pet.lifeSeconds % 1800 == 0) playerAddRyo(1);
  }

  // away on a mission: vitals above keep ticking, but no poop, needs or tantrums
  if (mission.active) {
    if (mission.left > 0) mission.left--;
    if (mission.left == 0) finishMission();
    return;
  }

  // poop
  if (!pet.sleeping) {
    if (pet.poopTimer > 0) pet.poopTimer--;
    if (pet.poopTimer == 0) {
      if (pet.poop < 4) {
        pet.poop++;
        emit(EV_POOP);
      }
      scheduleNextPoop();
    }
  }
  pet.poopNeglect = pet.poop >= 3 ? pet.poopNeglect + 1 : 0;
  pet.starving = pet.food <= 0 ? pet.starving + 1 : 0;

  // sickness and death
  if (!pet.sick && (pet.poopNeglect > 900 || pet.starving > 900)) {
    pet.sick = 1;
    pet.sickFor = 0;
    emit(EV_SICK);
  }
  if (pet.sick && ++pet.sickFor > SICK_DEATH) {
    die();
    return;
  }

  // tantrums (child and older): tap the pet to scold it before it escalates
  if (pet.stage >= S_CHILD && !pet.sleeping) {
    if (!pet.tantrum) {
      if (pet.tantrumIn > 0) pet.tantrumIn--;
      if (pet.tantrumIn == 0) {
        pet.tantrum = 1;
        pet.tantrumFor = 0;
        emit(EV_TANTRUM);
      }
    } else if (++pet.tantrumFor > 300) {
      pet.tantrum = 0;
      pet.careMistakes++;
      pet.discipline = clampf(pet.discipline - 5);
      scheduleNextTantrum();
      emit(EV_CARE_MISTAKE);
    }
  }

  // care mistakes: a need left unanswered for too long
  Need n = petNeed();
  if (n != (Need)pet.needKind) {
    pet.needKind = n;
    pet.needSince = pet.stageAge;
    pet.needPenalized = 0;
    if (n != NEED_NONE) emit(EV_CALL);
  } else if (n != NEED_NONE && n != NEED_TANTRUM && !pet.needPenalized && pet.stageAge - pet.needSince > NEED_GRACE) {
    pet.needPenalized = 1;
    pet.careMistakes++;
    emit(EV_CARE_MISTAKE);
  }

  // growing up
  if (pet.stage == S_BABY && pet.stageAge >= BABY_TIME) advanceStage(S_CHILD);
  else if (pet.stage == S_CHILD && pet.stageAge >= CHILD_TIME) advanceStage(S_TEEN);
  else if (pet.stage == S_TEEN && pet.stageAge >= TEEN_TIME) advanceStage(S_ADULT);
}

void petCatchUp(uint32_t seconds) {
  void (*hook)(PetEvent) = petEventHook;
  petEventHook = nullptr;  // no sounds or popups for time that already passed
  while (seconds--) petTick();
  petEventHook = hook;
}

// ---- actions ----
static ActionResult guard(bool needAwake = true) {
  if (pet.stage == S_EGG) return RES_EGG;
  if (pet.stage == S_GHOST) return RES_NOT_ALIVE;
  if (mission.active) return RES_AWAY;
  if (needAwake && pet.sleeping) return RES_ASLEEP;
  return RES_OK;
}

ActionResult petConsume(int food, int joy, int energy, int discipline) {
  ActionResult g = guard();
  if (g != RES_OK) return g;
  if (food >= 10 && pet.food > 95) return RES_FULL;  // a real meal needs room
  bool useful = (food > 0 && pet.food < 95) || (joy > 0 && pet.joy < 95) || (energy > 0 && pet.energy < 95) || discipline > 0;
  if (!useful) return RES_NOT_NEEDED;
  pet.food = clampf(pet.food + food);
  pet.joy = clampf(pet.joy + joy);
  pet.energy = clampf(pet.energy + energy);
  pet.discipline = clampf(pet.discipline + discipline);
  return RES_OK;
}

ActionResult petPlay() {
  ActionResult g = guard();
  if (g != RES_OK) return g;
  if (pet.food < 1) return RES_FAINTED;
  if (pet.energy < 10) return RES_TOO_TIRED;
  pet.joy = clampf(pet.joy + 18);
  pet.energy = clampf(pet.energy - 10);
  pet.food = clampf(pet.food - 4);
  pet.bond = clampf(pet.bond + 1);
  playerAddRyo(3);
  return RES_OK;
}

ActionResult petClean() {
  ActionResult g = guard(false);
  if (g != RES_OK) return g;
  if (pet.poop == 0) return RES_NOT_NEEDED;
  playerAddRyo(2 * pet.poop);
  pet.poop = 0;
  pet.poopNeglect = 0;
  pet.joy = clampf(pet.joy + 2);
  return RES_OK;
}

ActionResult petHeal() {
  ActionResult g = guard(false);
  if (g != RES_OK) return g;
  if (!pet.sick) return RES_NOT_NEEDED;
  pet.sick = 0;
  pet.sickFor = 0;
  pet.joy = clampf(pet.joy - 5);  // medicine is bitter
  return RES_OK;
}

ActionResult petToggleSleep() {
  ActionResult g = guard(false);
  if (g != RES_OK) return g;
  pet.sleeping = !pet.sleeping;
  if (!pet.sleeping && (petNight || petDark)) pet.stayUp = 1;
  return RES_OK;
}

ActionResult petStartMission(int kind) {
  ActionResult g = guard();
  if (g != RES_OK) return g;
  if (pet.stage < S_CHILD) return RES_TOO_YOUNG;
  if (pet.sick || pet.food < 30) return RES_UNWELL;
  if (pet.energy < 30) return RES_TOO_TIRED;
  mission.active = 1;
  mission.kind = kind;
  mission.left = MISSIONS[kind].seconds;
  return RES_OK;
}

ActionResult petCanPlayGame() {
  ActionResult g = guard();
  if (g != RES_OK) return g;
  if (pet.sick || pet.food < 10) return RES_UNWELL;
  if (pet.energy < 15) return RES_TOO_TIRED;
  return RES_OK;
}

void petApplyGame(int joy, int discipline, int energyCost, int ryo, float bond) {
  pet.joy = clampf(pet.joy + joy);
  pet.discipline = clampf(pet.discipline + discipline);
  pet.energy = clampf(pet.energy - energyCost);
  pet.food = clampf(pet.food - 4);
  pet.bond = clampf(pet.bond + bond);
  playerAddRyo(ryo);
  petSave();
  playerSave();
}

void petAlarmWake() {
  if (!pet.sleeping || pet.stage == S_EGG || pet.stage == S_GHOST) return;
  pet.sleeping = 0;
  darkSlept = false;
  if (petNight || petDark) pet.stayUp = 1;
  pet.joy = clampf(pet.joy - 2);
  emit(EV_WAKE);
}

void petTap() {
  if (pet.stage == S_EGG) {
    pet.warmth = min<int>(100, pet.warmth + 10);
  } else if (pet.stage == S_GHOST) {
    petNewEgg();
    emit(EV_EVOLVE);
  } else if (pet.sleeping) {
    pet.sleeping = 0;
    if (petNight || petDark) pet.stayUp = 1;
    pet.joy = clampf(pet.joy - 5);
    emit(EV_WAKE);
  } else if (pet.tantrum) {
    pet.tantrum = 0;
    pet.discipline = clampf(pet.discipline + 12);
    playerAddRyo(5);
    scheduleNextTantrum();
  } else {
    pet.bond = clampf(pet.bond + 0.5f);
    pet.joy = clampf(pet.joy + 1);
  }
}

uint32_t petStageSecondsLeft() {
  uint32_t total = pet.stage == S_BABY ? BABY_TIME : pet.stage == S_CHILD ? CHILD_TIME : pet.stage == S_TEEN ? TEEN_TIME : 0;
  return pet.stageAge >= total ? 0 : total - pet.stageAge;
}

// ---- names ----
const char *stageName() {
  static const char *names[] = {"EGG", "BABY", "CHILD", "TEEN", "ADULT", "SPIRIT"};
  return names[pet.stage];
}

const char *adultName(int form) {
  static const char *names[] = {"KITSUNE", "SAGE TOAD", "TANUKI", "SHADOW CAT", "RAIJU", "MOON SLUG",
                                "HAWK",    "SERPENT",   "MOON RABBIT", "AXOLOTL", "FROST PENGUIN", "CLOUD SHEEP"};
  return names[form];
}

const char *formName() { return pet.stage == S_ADULT ? adultName(pet.form) : stageName(); }
