#include "inventory.h"

#include <Preferences.h>

Player player;
static const uint8_t PLAYER_VERSION = 2;

//                         name        price kind     sprite          food joy energy disc  message
const ShopItemDef SHOP_ITEMS[SI_COUNT] = {
    {"ONIGIRI", 8, IK_FOOD, FOOD_ONIGIRI, 35, 3, 0, 0, "Yum!"},
    {"RAMEN", 20, IK_FOOD, FOOD_RAMEN, 50, 8, 0, 0, "Slurp!"},
    {"DANGO", 12, IK_FOOD, FOOD_DANGO, 8, 20, 0, 0, "Yay!"},
    {"CHAKRA", 15, IK_ITEM, ITEM_POTION, 0, 0, 40, 0, "Zoom!"},
    {"MEDICINE", 25, IK_ITEM, ITEM_MEDICINE, 0, 0, 0, 0, "Better!"},
    {"SCROLL", 30, IK_ITEM, ITEM_SCROLL, 0, 0, 0, 10, "Trained!"},
    {"MOCHI", 10, IK_FOOD, FOOD_MOCHI, 15, 12, 0, 0, "Squishy!"},
    {"TEMPURA", 16, IK_FOOD, FOOD_TEMPURA, 40, 6, 0, 0, "Crunchy!"},
    {"SUSHI", 22, IK_FOOD, FOOD_SUSHI, 45, 10, 0, 0, "Fresh!"},
    {"TAIYAKI", 14, IK_FOOD, FOOD_TAIYAKI, 25, 12, 0, 0, "Sweet!"},
    {"UDON", 18, IK_FOOD, FOOD_UDON, 45, 0, 5, 0, "Slurp!"},
    {"MATCHA", 12, IK_FOOD, FOOD_MATCHA, 5, 4, 20, 0, "Refreshing"},
    {"CAKE", 26, IK_FOOD, FOOD_CAKE, 20, 30, 0, 0, "Wow!"},
    {"BENTO", 30, IK_FOOD, FOOD_BENTO, 70, 8, 5, 0, "Feast!"},
    {"BERRY", 6, IK_FOOD, FOOD_BERRY, 10, 6, 0, 0, "Tasty!"},
};

const ShopItem SHOP_FOODS[12] = {SI_BERRY, SI_ONIGIRI, SI_MOCHI, SI_DANGO, SI_MATCHA, SI_TAIYAKI,
                                 SI_TEMPURA, SI_UDON, SI_RAMEN, SI_SUSHI, SI_CAKE, SI_BENTO};
const ShopItem SHOP_OTHERS[3] = {SI_POTION, SI_MEDICINE, SI_SCROLL};

const Sprite &itemSprite(ShopItem id) {
  const ShopItemDef &d = SHOP_ITEMS[id];
  return d.kind == IK_FOOD ? FOOD_FRAMES[d.sprite] : ITEM_FRAMES[d.sprite];
}

void itemEffectLines(ShopItem id, char l1[20], char l2[20]) {
  const ShopItemDef &d = SHOP_ITEMS[id];
  l1[0] = l2[0] = 0;
  if (id == SI_MEDICINE) {
    snprintf(l1, 20, "CURES SICKNESS");
    return;
  }
  struct { const char *n; int v; } fx[4] = {{"FOOD", d.food}, {"JOY", d.joy}, {"ENERGY", d.energy}, {"DISC", d.discipline}};
  int shown = 0;
  for (auto &e : fx) {
    if (!e.v) continue;
    char *dst = shown < 2 ? l1 : l2;
    size_t len = strlen(dst);
    snprintf(dst + len, 20 - len, "%s%s +%d", len ? "  " : "", e.n, e.v);
    shown++;
  }
}

static void newPlayer() {
  memset(&player, 0, sizeof(player));
  player.version = PLAYER_VERSION;
  player.ryo = 30;
  player.items[SI_MEDICINE] = 1;
  player.items[SI_ONIGIRI] = 1;
}

// Version 1 only had the first six items
struct PlayerV1 {
  uint8_t version;
  uint16_t ryo;
  uint8_t items[6];
};

void playerLoad() {
  Preferences prefs;
  prefs.begin("tama", true);
  size_t n = prefs.getBytesLength("player");
  bool ok = false;
  if (n == sizeof(Player)) {
    Player tmp;
    prefs.getBytes("player", &tmp, sizeof(Player));
    if (tmp.version == PLAYER_VERSION) {
      player = tmp;
      ok = true;
    }
  } else if (n == sizeof(PlayerV1)) {  // migrate: keep ryo and the original items
    PlayerV1 old;
    prefs.getBytes("player", &old, sizeof(PlayerV1));
    if (old.version == 1) {
      memset(&player, 0, sizeof(player));
      player.version = PLAYER_VERSION;
      player.ryo = old.ryo;
      memcpy(player.items, old.items, sizeof(old.items));
      ok = true;
    }
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

ActionResult eatRice() { return petConsume(20, 3, 0, 0); }

ActionResult useItem(ShopItem id) {
  if (player.items[id] == 0) return RES_NONE_LEFT;
  const ShopItemDef &d = SHOP_ITEMS[id];
  ActionResult r = id == SI_MEDICINE ? petHeal() : petConsume(d.food, d.joy, d.energy, d.discipline);
  if (r == RES_OK) player.items[id]--;
  return r;
}
