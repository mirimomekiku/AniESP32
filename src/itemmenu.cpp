#include "itemmenu.h"

#include "gfx.h"
#include "inventory.h"
#include "pet.h"
#include "petview.h"
#include "screen.h"

struct Row {
  const char *label;
  const Sprite *icon;
  int id;     // ShopItem, IM_FREE, IM_BACK or one of the A_* actions
  int count;  // -1 = free, -2 = no count shown
  const char *right;  // optional text on the right instead of the count
};
static const int IM_FREE = -1, IM_BACK = -2;
enum Act { A_QUICK = 100, A_SHURIKEN, A_SHELL, A_MISSION, A_MISSION0 = 110 };
static const int MX = 4, MW = 232, ROW_H = 34, ROW_STEP = 35, MY = 54;

static ItemMenuKind kind = IM_NONE;
static void (*redrawFn)() = nullptr;
static Row rows[5];
static int nrows = 0;

static void addRow(const char *label, const Sprite *icon, int id, int count, const char *right = nullptr) {
  rows[nrows++] = {label, icon, id, count, right};
}

static void addItemRow(ShopItem id) {
  const ShopItemDef &d = SHOP_ITEMS[id];
  addRow(d.name, &ITEM_FRAMES[d.sprite], id, player.items[id]);
}

static void draw() {
  gradPanel(&tft, MX - 2, MY - 4, MW + 4, nrows * ROW_STEP + 8, R_LAV, 6, 0);
  for (int i = 0; i < nrows; i++) {
    const Row &r = rows[i];
    int y = MY + i * ROW_STEP;
    bool empty = r.count == 0;
    Ramp ramp = r.id == IM_BACK ? R_PINK : (empty ? R_GREY : R_CREAM);
    gradPanel(&tft, MX + 2, y, MW - 4, ROW_H, ramp, 4, 0);
    if (r.icon) blitSprite(&tft, *r.icon, MX + 8, y + (ROW_H - 32) / 2);
    int tx = r.icon ? MX + 46 : MX + 12;
    drawText(&tft, FONT_SILK, r.label, tx, y + 12, 2, C_TEXT, C_WHITE, TXT_SHADOW);
    char buf[10] = "";
    if (r.right) snprintf(buf, sizeof(buf), "%s", r.right);
    else if (r.count == -1) snprintf(buf, sizeof(buf), "FREE");
    else if (r.count >= 0) snprintf(buf, sizeof(buf), "x%d", r.count);
    if (buf[0]) drawText(&tft, FONT_CHUNK, buf, MX + MW - 12, y + 10, 2, empty ? R_RED.bot : C_TEXT, C_WHITE, TXT_RIGHT | TXT_SHADOW);
  }
}

void imOpen(ItemMenuKind k, void (*redraw)()) {
  kind = k;
  redrawFn = redraw;
  nrows = 0;
  if (k == IM_FEED) {
    addRow("RICE", &UI_FRAMES[UI_BOWL], IM_FREE, -1);
    addItemRow(SI_ONIGIRI);
    addItemRow(SI_RAMEN);
    addItemRow(SI_DANGO);
  } else if (k == IM_PLAY) {
    addRow("QUICK", &UI_FRAMES[UI_BALL], A_QUICK, -2);
    addRow("SHURIKEN", &ITEM_FRAMES[ITEM_SHURIKEN], A_SHURIKEN, -2);
    addRow("SHELL", &ITEM_FRAMES[ITEM_SMOKE], A_SHELL, -2);
    addRow("MISSION", &ITEM_FRAMES[ITEM_SCROLL], A_MISSION, -2);
  } else if (k == IM_MISSION) {
    static char durations[MISSION_COUNT][14];
    for (int i = 0; i < MISSION_COUNT; i++) {
      snprintf(durations[i], sizeof(durations[i]), "%lu MIN", (unsigned long)(MISSIONS[i].seconds / 60));
      addRow(MISSIONS[i].name, &ITEM_FRAMES[ITEM_KUNAI], A_MISSION0 + i, -2, durations[i]);
    }
  } else {
    addItemRow(SI_MEDICINE);
    addItemRow(SI_POTION);
    addItemRow(SI_SCROLL);
  }
  addRow("BACK", nullptr, IM_BACK, -2);
  draw();
}

bool imActive() { return kind != IM_NONE; }

static void close() {
  kind = IM_NONE;
  if (redrawFn) redrawFn();
}

bool imTouch(int x, int y) {
  if (kind == IM_NONE) return false;
  int i = (y - MY) / ROW_STEP;
  if (y < MY || i >= nrows || x < MX || x >= MX + MW) return true;  // swallow stray touches
  const Row r = rows[i];
  close();
  if (r.id == IM_BACK) return true;

  if (r.id >= A_QUICK) {
    if (r.id == A_QUICK) {
      pvFeedback(petPlay(), "Wheee!", ANIM_PLAY, 1800);
    } else if (r.id == A_MISSION) {
      imOpen(IM_MISSION, redrawFn);
    } else if (r.id >= A_MISSION0) {
      ActionResult res = petStartMission(r.id - A_MISSION0);
      pvFeedback(res, "Off we go!", ANIM_NONE, 0);
      if (res == RES_OK) petSave();
    } else {
      ActionResult res = petCanPlayGame();
      if (res == RES_OK) gotoScreen(r.id == A_SHURIKEN ? SCR_SHURIKEN : SCR_SHELL);
      else pvFeedback(res, "", ANIM_NONE, 0);
    }
    return true;
  }
  if (r.id == IM_FREE) {
    pvSetFood(r.icon);
    pvFeedback(eatRice(), "Yum!", ANIM_EAT, 1400);
    return true;
  }
  ShopItem id = (ShopItem)r.id;
  ActionResult res = useItem(id);
  bool food = id == SI_ONIGIRI || id == SI_RAMEN || id == SI_DANGO;
  pvSetFood(r.icon);
  pvFeedback(res, SHOP_ITEMS[id].okMsg, food ? ANIM_EAT : ANIM_CLEAN, food ? 1400 : 700);
  playerSave();
  petSave();
  return true;
}
