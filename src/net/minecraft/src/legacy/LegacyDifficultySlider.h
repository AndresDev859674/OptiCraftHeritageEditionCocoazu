#pragma once

#include "LegacyOptionSlider.h"

// Difficulty is a cycling option, not one of the float options the slider normally
// drives, so it reads and writes GameSettings::difficulty directly and snaps to its
// four stops. Everything else -- the track, the knob, dragging, the keyboard step --
// comes from LegacyOptionSlider.
class LegacyDifficultySlider : public LegacyOptionSlider
{
public:
    LegacyDifficultySlider(int_t id, int_t x, int_t y, int_t width, int_t height,
        GameSettings *settings);

    // A hardcore world has its difficulty fixed; the slider then shows that instead
    // of the current setting and the screen disables it.
    void setHardcore(bool hardcore);

protected:
    float_t readValue() const override;
    void writeValue(float_t value) override;
    std::string buildLabel() const override;
    float_t keyboardStep() const override;

private:
    bool hardcore;
};
