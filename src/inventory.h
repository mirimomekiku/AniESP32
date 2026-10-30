// Player-level state that outlives a single pet: ryo (money) and the bag.
#pragma once
#include <Arduino.h>

#include "assets.h"
#include "pet.h"

// The first six ids are the original items and must keep their numbers (older saves store counts by id).
enum ShopItem : uint8_t {
  SI_ONIGIRI, SI_RAMEN, SI_DANGO, SI_POTION, SI_MEDICINE, SI_SCROLL,
  SI_MOCHI, SI_TEMPURA, SI_SUSHI, SI_TAIYAKI, SI_UDON, SI_MATCHA, SI_CAKE, SI_BENTO, SI_BERRY,
  SI_COUNT
};

enum ItemKind : uint8_t { IK_FOOD, IK_ITEM };

struct ShopItemDef {
  const char *name;
  uint16_t price;
  ItemKind kind;
  uint8_t sprite;  // FOOD_* for foods, ITEM_* for other items
  int8_t food, joy, energy, discipline;  // what it gives
  const char *okMsg;
};

extern const ShopItemDef SHOP_ITEMS[SI_COUNT];

const Sprite &itemSprite(ShopItem id);
// What an item does, as (icon, value) pairs for the shop cards: food/joy/energy/discipline gains, or a cure.
enum EffectKind : uint8_t { EF_FOOD, EF_JOY, EF_ENERGY, EF_DISCIPLINE, EF_CURE };
struct ItemEffect {
  EffectKind kind;
  int8_t value;
};
int itemEffects(ShopItem id, ItemEffect out[4]);  // returns how many
const Sprite &effectIcon(EffectKind k);

// Shop order: cheapest first within each category
extern const ShopItem SHOP_FOODS[12];
extern const ShopItem SHOP_OTHERS[3];

const int MAX_STACK = 9;
const int MAX_RYO = 9999;

struct Player {
  uint8_t version;
  uint16_t ryo;
  uint8_t items[SI_COUNT];
};

extern Player player;

void playerLoad();
void playerSave();
void playerAddRyo(int n);
bool playerSpendRyo(int n);

ActionResult useItem(ShopItem id);  // consumes one on success
ActionResult eatRice();             // the free basic meal
