// Time keeping (no RTC: restored from flash, optionally synced over NTP) and the clock screen.
#pragma once
#include <Arduino.h>
#include <time.h>

void timeInit();
void timeSave();
void timeUpdate();  // run every loop: finishes an NTP sync, triggers pet catch-up

extern bool use24h;
void clockLoadPrefs();

int clockHour();       // local hour 0..23
bool clockIsNight();   // 21:00-06:00, the pet sleeps

bool alarmRinging();
void alarmDismiss();  // silence the alarm (any tap does this from main.cpp)
