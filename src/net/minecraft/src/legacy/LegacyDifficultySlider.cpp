#include "LegacyDifficultySlider.h"

#include <algorithm>

#include "net/minecraft/src/EnumOptions.h"
#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/StringTranslate.h"

namespace
{
// Peaceful, Easy, Normal, Hard: four stops, so three gaps across the track.
constexpr int_t DIFFICULTY_STEPS = 3;

int_t clampDifficulty(int_t value)
{
    return std::max<int_t>(0, std::min<int_t>(DIFFICULTY_STEPS, value));
}
}

LegacyDifficultySlider::LegacyDifficultySlider(int_t id, int_t x, int_t y, int_t width, int_t height,
    GameSettings *settingsValue)
    : LegacyOptionSlider(id, x, y, width, height, settingsValue, EnumOptions::DIFFICULTY),
      hardcore(false)
{
    refreshFromSettings();
}

void LegacyDifficultySlider::setHardcore(bool hardcoreValue)
{
    hardcore = hardcoreValue;
    refreshFromSettings();
}

float_t LegacyDifficultySlider::readValue() const
{
    return static_cast<float_t>(clampDifficulty(settings->difficulty)) /
        static_cast<float_t>(DIFFICULTY_STEPS);
}

void LegacyDifficultySlider::writeValue(float_t value)
{
    // Round to the nearest stop so the knob lands on a difficulty rather than
    // between two of them.
    const float_t scaled = value * static_cast<float_t>(DIFFICULTY_STEPS);
    settings->difficulty = clampDifficulty(static_cast<int_t>(scaled + 0.5f));
}

std::string LegacyDifficultySlider::buildLabel() const
{
    StringTranslate *translate = StringTranslate::getInstance();
    if (hardcore)
    {
        return translate->translateKey("options.difficulty") + ": " +
            translate->translateKey("options.difficulty.hardcore");
    }
    return settings->getKeyBinding(EnumOptions::DIFFICULTY);
}

float_t LegacyDifficultySlider::keyboardStep() const
{
    return 1.0f / static_cast<float_t>(DIFFICULTY_STEPS);
}
