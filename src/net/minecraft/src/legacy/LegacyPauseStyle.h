#pragma once

#include "java/Type.h"

int_t legacyPauseOverlayTopColor();
int_t legacyPauseOverlayBottomColor();
float_t legacyPauseButtonOpacity();
int_t legacyPauseButtonCount();
bool legacyPauseInputDelayElapsed(long_t openedAtMillis, long_t nowMillis);
