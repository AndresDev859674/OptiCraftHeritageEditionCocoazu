#pragma once

#include "java/Type.h"

class Minecraft;

// Draws the standalone console cursor texture centred at the requested point.
// This is shared by container-slot navigation and any UI path that needs the
// cursor artwork without taking ownership of the platform mouse position.
void legacyDrawSelectionCursorCentered(Minecraft *mc, int_t centerX, int_t centerY,
    int_t size, float_t zLevel);

// Draws the same standalone cursor texture used by the console software pointer
// as a row-selection marker. Pointer-hover selection keeps using the normal
// control hover visuals and does not call this helper.
void legacyDrawSelectionCursor(Minecraft *mc, int_t controlX, int_t controlY,
    int_t controlHeight, float_t zLevel);
