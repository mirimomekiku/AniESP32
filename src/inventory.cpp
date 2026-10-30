#include "inventory.h"

#include <Preferences.h>

#include "assets.h"

Player player;
static const uint8_t PLAYER_VERSION = 1;

const ShopItemDef SHOP_ITEMS[SI_COUNT] = {
    {"ONIGIRI", 8, ITEM_ONIGIRI, "FOOD +35", "Yum!"},
    {"RAMEN", 20, ITEM_RAMEN, "FOOD +50 JOY +8", "Slurp!"},
    {"DANGO", 12, ITEM_DANGO, "JOY +20", "Yay!"},
    {"CHAKRA", 15, ITEM_POTION, "ENERGY +40", "Zoom!"},
    {"MEDICINE", 25, ITEM_MEDICINE, "CURES SICKNESS", "Better!"},
    {"SCROLL", 30, ITEM_SCROLL, "DISCIPLINE +10", "Trained!"},
};

static void newPlayer() {
  memset(&player, 0, sizeof(player));
  player.version = PLAYER_VERSION;
  player.ryo = 30;
  player.items[SI_MEDICINE] = 1;
  player.items[SI_ONIGIRI] = 1;
}

void playerLoad() {
  Preferences prefs;
  prefs.begin("tama", true);
  bool ok = prefs.getBytesLength("player") == sizeof(Player);
  if (ok) {
    Player tmp;
    prefs.getBytes("player", &tmp, sizeof(Player));
    if (tmp.version == PLAYER_VERSION) player = tmp;
    else ok = false;
  }
  prefs.end();
  if (!ok) newPlayer();
}

void playerSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putBytes("player", &player, sizeof(Player));
  prefs.end();
}

void playerAddRyo(int n) { player.ryo = constrain((int)player.ryo + n, 0, MAX_RYO); }

bool playerSpendRyo(int n) {
  if (player.ryo < n) return false;
  player.ryo -= n;
  return true;
}

ActionResult eatRice() { return petEat(20, 3); }

ActionResult useItem(ShopItem id) {
  if (player.items[id] == 0) return RES_NONE_LEFT;
  ActionResult r = RES_NOT_NEEDED;
  switch (id) {
    case SI_ONIGIRI: r = petEat(35, 3); break;
    case SI_RAMEN: r = petEat(50, 8); break;
    case SI_DANGO: r = petEat(8, 20); break;
    case SI_POTION: r = petRestoreEnergy(40); break;
    case SI_MEDICINE: r = petHeal(); break;
    case SI_SCROLL: r = petTrain(10); break;
    default: break;
  }
  if (r == RES_OK) player.items[id]--;
  return r;
}
