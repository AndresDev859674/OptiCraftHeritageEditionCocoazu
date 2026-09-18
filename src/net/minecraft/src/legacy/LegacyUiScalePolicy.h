#pragma once

#include "java/Type.h"

inline int_t legacyUiLargeGuiScale()
{
    return 2;
}

inline int_t legacyUiClampGuiScale(int_t value)
{
    if (value < 0)
        return 0;
    if (value > 3)
        return 3;
    return value;
}

inline int_t legacyUiEffectiveGuiScale(bool legacyEnabled, int_t preferredScale)
{
    return legacyEnabled ? legacyUiLargeGuiScale() : legacyUiClampGuiScale(preferredScale);
}

inline int_t legacyUiRestoreScaleAfterLoad(bool legacyEnabled, int_t loadedScale,
    bool savedRestoreWasLoaded, int_t savedRestore)
{
    if (!legacyEnabled)
        return legacyUiClampGuiScale(loadedScale);
    return savedRestoreWasLoaded
        ? legacyUiClampGuiScale(savedRestore)
        : legacyUiClampGuiScale(loadedScale);
}
