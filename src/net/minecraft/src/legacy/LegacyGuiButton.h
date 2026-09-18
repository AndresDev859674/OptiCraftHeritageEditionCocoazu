#pragma once

#include "net/minecraft/src/GuiButton.h"

class LegacyGuiButton : public GuiButton
{
public:
    LegacyGuiButton(int_t id, int_t x, int_t y, int_t width, int_t height, const std::string &text,
        float_t opacity = 1.0f);

    void drawButton(Minecraft *mc, int_t mouseX, int_t mouseY) override;
    void setSelected(bool selectedValue);
    void setKeyboardSelected(bool selectedValue) override;

private:
    float_t opacity;
    bool selected;
};
