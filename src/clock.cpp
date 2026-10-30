#include "clock.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include "config.h"
#include "gfx.h"
#include "hw.h"
#include "pet.h"
#include "screen.h"

bool use24h = true;

// ---- alarm ----
static int alarmH = 7, alarmM = 0;
static bool alarmOn = false;
static bool ringing = false;
static uint32_t ringStart = 0, lastRingBeep = 0;
static int lastRungKey = -1;
static bool clockDirty = true;

// ---- time keeping ----
static bool setMode = false;
static bool epochWasSaved = false;
static time_t bootEpoch = 0;
static uint32_t bootMillis = 0;

enum SyncState { SYNC_IDLE, SYNC_RUN, SYNC_OK, SYNC_FAIL };
static SyncState syncState = SYNC_IDLE;
static volatile bool gotNtp = false;
static uint32_t syncStart = 0;
static bool caughtUp = false;

static time_t buildTime() {
  static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int d, y, hh, mm, ss;
  sscanf(__DATE__, "%3s %d %d", mon, &d, &y);
  sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
  struct tm t = {};
  const char *p = strstr(months, mon);
  t.tm_mon = p ? (p - months) / 3 : 0;
  t.tm_mday = d;
  t.tm_year = y - 1900;
  t.tm_hour = hh;
  t.tm_min = mm;
  t.tm_sec = ss;
  t.tm_isdst = -1;
  return mktime(&t);
}

static void setEpoch(time_t t) {
  struct timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
}

static void startSync() {
  if (strlen(WIFI_SSID) == 0) {
    syncState = SYNC_FAIL;
    clockDirty = true;
    return;
  }
  gotNtp = false;
  sntp_set_time_sync_notification_cb([](struct timeval *) { gotNtp = true; });
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTzTime(TZ_POSIX, "pool.ntp.org", "time.nist.gov");
  syncState = SYNC_RUN;
  syncStart = millis();
  clockDirty = true;
}

void timeInit() {
  setenv("TZ", TZ_POSIX, 1);
  tzset();
  // No RTC: restore the last saved time, but never go earlier than this firmware's build time
  Preferences prefs;
  prefs.begin("tama", true);
  time_t saved = (time_t)prefs.getULong("epoch", 0);
  prefs.end();
  time_t floorT = buildTime();
  epochWasSaved = saved > floorT;
  bootEpoch = epochWasSaved ? saved : floorT;
  bootMillis = millis();
  setEpoch(bootEpoch);
  if (strlen(WIFI_SSID)) startSync();
}

void timeSave() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putULong("epoch", (uint32_t)time(nullptr));
  prefs.end();
}

void clockLoadPrefs() {
  Preferences prefs;
  prefs.begin("tama", true);
  use24h = prefs.getBool("h24", true);
  alarmH = prefs.getInt("alH", 7);
  alarmM = prefs.getInt("alM", 0);
  alarmOn = prefs.getBool("alOn", false);
  prefs.end();
}

static void saveAlarm() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putInt("alH", alarmH);
  prefs.putInt("alM", alarmM);
  prefs.putBool("alOn", alarmOn);
  prefs.end();
}

bool alarmRinging() { return ringing; }

void alarmDismiss() {
  ringing = false;
  hwAttention(false);
  clockDirty = true;
}

static void alarmTick() {
  uint32_t now = millis();
  if (ringing) {
    if (now - ringStart > 60000) { alarmDismiss(); return; }
    if (now - lastRingBeep > 500) {
      lastRingBeep = now;
      hwBeep((now / 500) % 2 ? 1175 : 880, 300);
      hwTouched();  // keep the screen bright
    }
    return;
  }
  if (!alarmOn) return;
  static uint32_t lastCheck = 0;
  if (now - lastCheck < 1000) return;
  lastCheck = now;
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  int key = tm.tm_yday * 1440 + tm.tm_hour * 60 + tm.tm_min;
  if (tm.tm_hour == alarmH && tm.tm_min == alarmM && key != lastRungKey) {
    lastRungKey = key;
    ringing = true;
    ringStart = now;
    lastRingBeep = 0;
    hwAttention(true);
    petAlarmWake();
    gotoScreen(SCR_CLOCK);
    clockDirty = true;
  }
}

static void saveUse24h() {
  Preferences prefs;
  prefs.begin("tama", false);
  prefs.putBool("h24", use24h);
  prefs.end();
}

void timeUpdate() {
  alarmTick();
  if (syncState != SYNC_RUN) return;
  bool timeout = millis() - syncStart > 20000;
  if (!gotNtp && !timeout) return;

  syncState = gotNtp ? SYNC_OK : SYNC_FAIL;
  if (gotNtp) {
    timeSave();
    // The pet kept living while the device was off: simulate it once, capped at 8 hours.
    if (!caughtUp && epochWasSaved) {
      time_t now = time(nullptr);
      long offline = (long)(now - bootEpoch) - (long)((millis() - bootMillis) / 1000);
      if (offline > 60) {
        petCatchUp(min<long>(offline, 8L * 3600) * TIME_SCALE);
        petSave();
      }
    }
    caughtUp = true;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  clockDirty = true;
}

int clockHour() {
  time_t t = time(nullptr);
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_hour;
}

bool clockIsNight() {
  int h = clockHour();
  return h >= 21 || h < 6;
}

// ---- clock screen ----
static const int CLK_VX = 0, CLK_VY = 30, CLK_VW = 240, CLK_VH = 160;
static const char *DAYS[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
static ButtonRow buttons;
static int lastShownSec = -1;
static bool alarmMode = false;

static void setButtons() {
  if (alarmMode) {
    BtnDef defs[4] = {{"OK", nullptr, R_MINT}, {"+H", nullptr, R_SKY}, {"+M", nullptr, R_SKY}, {alarmOn ? "ON" : "OFF", nullptr, alarmOn ? R_BUTTER : R_GREY}};
    buttons.set(defs, 4);
  } else if (setMode) {
    BtnDef defs[3] = {{"OK", nullptr, R_MINT}, {"+H", nullptr, R_SKY}, {"+M", nullptr, R_SKY}};
    buttons.set(defs, 3);
  } else {
    BtnDef defs[4] = {{"SET", &UI_FRAMES[UI_CLOCK], R_MINT},
                      {"SYNC", &UI_FRAMES[UI_BOLT], R_SKY},
                      {use24h ? "24H" : "12H", nullptr, R_BUTTER},
                      {"ALARM", &UI_FRAMES[UI_BELL], R_PINK}};
    buttons.set(defs, 4);
  }
  buttons.draw();
}

static void clockEnter() {
  setMode = false;
  alarmMode = false;
  gfxMakeView(CLK_VW, CLK_VH);
  drawBg(BG_CLOCK, BG_CLOCK_PAL);
  drawNav(TAB_CLOCK);
  setButtons();
  clockDirty = true;
}

static void clockDraw() {
  time_t nowT = time(nullptr);
  struct tm tm;
  localtime_r(&nowT, &tm);
  bool flash = ringing && (millis() / 400) % 2;
  static uint32_t lastRingDraw = 0;
  bool ringRedraw = ringing && millis() - lastRingDraw > 200;
  if (!clockDirty && tm.tm_sec == lastShownSec && !ringRedraw) return;
  lastRingDraw = millis();
  lastShownSec = tm.tm_sec;
  clockDirty = false;

  copyBgToView(BG_CLOCK, BG_CLOCK_PAL, CLK_VX, CLK_VY, CLK_VW, CLK_VH);

  // frosted night-glass panel behind the digits
  static Ramp glass = {rgb(86, 70, 170), rgb(52, 40, 120)};
  gradPanel(&view, 6, 4, 228, 70, glass, 6, 0);

  int hour = alarmMode ? alarmH : tm.tm_hour;
  int minute = alarmMode ? alarmM : tm.tm_min;
  bool pm = hour >= 12;
  if (!use24h) hour = hour % 12 == 0 ? 12 : hour % 12;

  char buf[8];
  bool colon = setMode || alarmMode || (tm.tm_sec % 2 == 0);
  snprintf(buf, sizeof(buf), use24h ? "%02d%c%02d" : "%2d%c%02d", hour, colon ? ':' : ' ', minute);
  drawText(&view, FONT_CHUNK, buf, 120, 20, 5, flash ? R_PINK.top : C_CREAM, C_OUT, TXT_CENTER | TXT_OUTLINE);
  if (!use24h) drawText(&view, FONT_SILK, pm ? "PM" : "AM", 224, 8, 1, R_BUTTER.top, C_OUT, TXT_RIGHT | TXT_OUTLINE);

  if (alarmMode) {
    drawText(&view, FONT_CHUNK, alarmOn ? "ALARM ON" : "ALARM OFF", 120, 86, 2, alarmOn ? R_MINT.top : R_PINK.top, C_OUT, TXT_CENTER | TXT_OUTLINE);
  } else {
    snprintf(buf, sizeof(buf), "%02d", tm.tm_sec);
    drawText(&view, FONT_CHUNK, buf, 120, 84, 3, R_PINK.top, C_OUT, TXT_CENTER | TXT_OUTLINE);
  }

  char date[24];
  snprintf(date, sizeof(date), "%s %02d %s %d", DAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon], tm.tm_year + 1900);
  drawText(&view, FONT_SILK, date, 120, 118, 2, C_WHITE, C_OUT, TXT_CENTER | TXT_OUTLINE);

  char status[24] = "";
  if (ringing) snprintf(status, sizeof(status), "WAKE UP! TAP TO STOP");
  else if (alarmMode) snprintf(status, sizeof(status), "SET ALARM");
  else if (setMode) snprintf(status, sizeof(status), "SET THE TIME");
  else if (syncState == SYNC_RUN) snprintf(status, sizeof(status), "SYNCING...");
  else if (syncState == SYNC_OK) snprintf(status, sizeof(status), "SYNCED");
  else if (syncState == SYNC_FAIL) snprintf(status, sizeof(status), "%s", strlen(WIFI_SSID) ? "SYNC FAILED" : "NO WIFI SET");
  else if (alarmOn) snprintf(status, sizeof(status), "ALARM %02d:%02d", alarmH, alarmM);
  if (status[0]) drawText(&view, FONT_SILK, status, 120, 142, 1, ringing ? C_WHITE : R_BUTTER.top, C_OUT, TXT_CENTER | TXT_OUTLINE);

  // light sensor read-out, handy to check that the LDR works
#if USE_LDR
  snprintf(buf, sizeof(buf), ldrReliable() ? "LDR %d" : "LDR ?", ldrRaw());
  drawText(&view, FONT_SILK, buf, 236, 150, 1, R_LAV.top, C_OUT, TXT_RIGHT | TXT_OUTLINE);
#endif

  view.pushSprite(CLK_VX, CLK_VY);
}

static void clockUpdate() {
  buttons.update();
  clockDraw();
}

static void clockTouch(int x, int y) {
  int i = buttons.hit(x, y);
  if (i < 0) return;
  buttons.press(i);
  if (alarmMode) {
    if (i == 0) { alarmMode = false; saveAlarm(); setButtons(); }
    else if (i == 1) alarmH = (alarmH + 1) % 24;
    else if (i == 2) alarmM = (alarmM + 1) % 60;
    else { alarmOn = !alarmOn; setButtons(); }
  } else if (setMode) {
    if (i == 0) { setMode = false; timeSave(); setButtons(); }
    else if (i == 1) setEpoch(time(nullptr) + 3600);
    else setEpoch(time(nullptr) + 60);
  } else if (i == 0) {
    setMode = true;
    setButtons();
  } else if (i == 1) {
    startSync();
  } else if (i == 2) {
    use24h = !use24h;
    saveUse24h();
    setButtons();
  } else {
    alarmMode = true;
    setButtons();
  }
  clockDirty = true;
}

const Screen CLOCK_SCREEN = {clockEnter, clockUpdate, clockTouch};
