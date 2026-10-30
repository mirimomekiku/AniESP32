// Player-level state that outlives a single pet: ryo (money) and the bag.
#pragma once
#include <Arduino.h>

#include "pet.h"

enum ShopItem : uint8_t { SI_ONIGIRI, SI_RAMEN, SI_DANGO, SI_POTION, SI_MEDICINE, SI_SCROLL, SI_COUNT };

struct ShopItemDef {
  const char *name;
  uint16_t price;
  uint8_t sprite;  // ITEM_* index in assets.h
  const char *blurb;
  const char *okMsg;
};

extern const ShopItemDef SHOP_ITEMS[SI_COUNT];

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
