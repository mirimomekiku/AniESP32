// Album ("DEX"): which adult forms this device has raised so far.
#pragma once
#include <Arduino.h>

void albumLoad();
void albumUnlock(uint8_t form);
bool albumHas(uint8_t form);
int albumCount();
uint16_t albumRaised();  // adults raised in total
const char *albumHint(uint8_t form);
