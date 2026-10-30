// Ninja Tamagotchi + pixel clock for the ESP32-2432S028 "CYD" (portrait, 240x320)
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "album.h"
#include "clock.h"
#include "config.h"
#include "gfx.h"
#include "hw.h"
#include "inventory.h"
#include "pet.h"
#include "screen.h"

// Touch controller uses its own SPI bus on the CYD
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

static const Screen *screens[SCR_COUNT] = {&VILLAGE_SCREEN, &MENU_SCREEN,  &CLOCK_SCREEN,   &SHOP_SCREEN,   &ALBUM_SCREEN,  &STATS_SCREEN,
                                           &FEED_SCREEN,    &PLAY_SCREEN,  &CLEAN_SCREEN,   &BAG_SCREEN,    &SLEEP_SCREEN,
                                           &SHURIKEN_SCREEN, &SHELL_SCREEN, &DIAG_SCREEN};
static int current = 0;
static bool wasTouched = false;
static uint32_t lastTick = 0, lastSave = 0;

// Landscape calibration from the CYD examples, remapped for TFT rotation 0 (portrait)
static void readTouch(int &x, int &y) {
  TS_Point p = ts.getPoint();
  int xl = map(p.x, 200, 3700, 0, 320);
  int yl = map(p.y, 240, 3800, 0, 240);
  x = constrain(239 - yl, 0, 239);
  y = constrain(xl, 0, 319);
}

// Screens fade through black: fade out, redraw while dark, fade back in
static void switchTo(int s, bool fadeOut = true) {
  if (fadeOut) hwFadeTo(0, 120);
  current = s;
  screens[current]->enter();
  hwFadeTo(hwTargetBacklight(), 240);
}

void gotoScreen(int id) { switchTo(id); }

static void handleTouch() {
  bool touched = ts.tirqTouched() && ts.touched();
  if (touched) hwTouched();
  if (touched && !wasTouched) {
    if (alarmRinging()) {  // the first tap only silences the alarm
      alarmDismiss();
      wasTouched = true;
      return;
    }
    int x, y;
    readTouch(x, y);
    if (current <= SCR_CLOCK && y >= NAV_Y) {
      int target = constrain(x / 80, 0, TAB_COUNT - 1);  // TAB_* order matches the first three ScreenIds
      if (target != current) switchTo(target);
    } else {
      screens[current]->touch(x, y);
    }
  }
  wasTouched = touched;
}

void setup() {
  Serial.begin(115200);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  gfxInit();
  hwInit();
  clockLoadPrefs();
  timeInit();
  petLoad();
  playerLoad();
  albumLoad();
  if (pet.stage == S_ADULT) albumUnlock(pet.form);
  villageInit();

  hwFadeTo(0, 1);
  switchTo(0, false);
  lastTick = lastSave = millis();
}

void loop() {
  uint32_t now = millis();

  while (now - lastTick >= 1000) {
    lastTick += 1000;
    petNight = clockIsNight();
    petDark = ldrIsDark();
    for (int i = 0; i < TIME_SCALE; i++) petTick();
  }
  if (now - lastSave > 60000) {
    lastSave = now;
    petSave();
    playerSave();
    timeSave();
    ldrSave();
  }

  timeUpdate();
  handleTouch();
  ldrUpdate();
  hwUpdate(ldrIsDark() || clockIsNight() || pet.sleeping);
  screens[current]->update();
}
