// Pop-up lists (feed, bag, play, missions) opened from the room screens.
#pragma once
#include <Arduino.h>

enum ItemMenuKind { IM_NONE, IM_FEED, IM_BAG, IM_PLAY, IM_MISSION };

// Draws the list over the current screen. redraw() restores the screen when the list closes.
void imOpen(ItemMenuKind kind, void (*redraw)());
bool imActive();
bool imTouch(int x, int y);  // true when the touch was consumed by the list
