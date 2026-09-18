#pragma once

#include "net/minecraft/src/GuiButton.h"

class EnumOptions;
class GameSettings;

class LegacyOptionSlider : public GuiButton
{
public:
    LegacyOptionSlider(int_t id, int_t x, int_t y, int_t width, int_t height,
        GameSettings *settings, EnumOptions *option);

    void drawButton(Minecraft *mc, int_t mouseX, int_t mouseY) override;
    bool mousePressed(Minecraft *mc, int_t mouseX, int_t mouseY) override;
    void mouseReleased(int_t mouseX, int_t mouseY) override;

    void refreshFromSettings();
    void setKeyboardSelected(bool selectedValue) override;
    bool adjustKeyboard(Minecraft *mc, int_t direction) override;

protected:
    void mouseDragged(Minecraft *mc, int_t mouseX, int_t mouseY) override;

    // Where the slider gets and puts its value, and what it writes on the track.
    // The defaults drive a float GameSettings option; a subclass overrides them to
    // drive something the option table does not expose as a float, without
    // repeating the track drawing or the pointer handling.
    virtual float_t readValue() const;
    virtual void writeValue(float_t value);
    virtual std::string buildLabel() const;
    // One press of left or right. A discrete slider needs a step large enough to
    // reach the next stop, or the value rounds back and nothing moves.
    virtual float_t keyboardStep() const;

    GameSettings *settings;
    EnumOptions *option;

private:
    void updateFromMouse(Minecraft *mc, int_t mouseX);
    void refreshLabel();

    float_t sliderValue;
    bool dragging;
    bool selected;
};
