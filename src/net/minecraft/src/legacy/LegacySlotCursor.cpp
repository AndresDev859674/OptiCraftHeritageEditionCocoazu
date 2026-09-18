#include "LegacySlotCursor.h"

#include "LegacySelectionCursor.h"
#include "net/minecraft/src/Slot.h"

namespace
{
constexpr int_t SLOT_CURSOR_SIZE = 20;
constexpr int_t SLOT_CENTER = 8;
}

void legacyDrawSlotCursor(Minecraft *mc, const Slot *slot, float_t zLevel)
{
    if (mc == nullptr || slot == nullptr)
        return;

    const int_t centerX = slot->xDisplayPosition + SLOT_CENTER;
    const int_t centerY = slot->yDisplayPosition + SLOT_CENTER;
    // Make the slot-selection cursor visibly overlap the slot item
    // instead of disappearing into 16x16 icons.
    legacyDrawSelectionCursorCentered(mc, centerX, centerY,
        SLOT_CURSOR_SIZE, zLevel + 64.0f);
}
