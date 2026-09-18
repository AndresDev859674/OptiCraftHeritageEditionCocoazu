#pragma once

#include <string>
#include <vector>

#include "java/Type.h"

class GameSettings;

enum class LegacyControlsBindingKind
{
    KeyBinding,
    WiiFamilyBinding
};

enum class LegacyControlsWiiFamily
{
    None,
    GameCube,
    Wiimote,
    Classic
};

struct LegacyControlsBindingRow
{
    LegacyControlsBindingKind kind = LegacyControlsBindingKind::KeyBinding;
    std::string label;
    int_t bindingIndex = -1;
    int_t action = -1;
    LegacyControlsWiiFamily wiiFamily = LegacyControlsWiiFamily::None;
    int_t *wiiField = nullptr;
};

std::vector<LegacyControlsBindingRow> legacyControlsRows(GameSettings *settings);
bool legacyControlsBindingConflicts(GameSettings *settings, const LegacyControlsBindingRow &row);
std::string legacyControlsBindingLabel(GameSettings *settings, const LegacyControlsBindingRow &row);
bool legacyControlsApplyCapturedKey(GameSettings *settings, const LegacyControlsBindingRow &row, int_t key);
