#pragma once

#include <string>

class GameSettings;

enum class LegacyControlAction
{
    Inventory,
    Drop,
    Jump,
    Attack,
    Use
};

std::string legacyControlPromptLabel(const GameSettings &settings, LegacyControlAction action);
