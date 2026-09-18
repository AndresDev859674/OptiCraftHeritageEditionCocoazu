#pragma once

#include "net/minecraft/src/GuiButton.h"

class LegacyOptionCheckbox : public GuiButton
{
public:
    LegacyOptionCheckbox(int_t id, int_t x, int_t y, int_t width, int_t height,
        const std::string &label, bool checked);

    void drawButton(Minecraft *mc, int_t mouseX, int_t mouseY) override;
    void setChecked(bool checked);
    bool isChecked() const;
    void setKeyboardSelected(bool selectedValue) override;

private:
    // Used when /legacy/tickbox*.png or /legacy/tick.png is not installed.
    void drawProceduralBox(int_t boxX, int_t boxY, int_t boxSize, bool hovered);
    void drawProceduralTick(int_t boxX, int_t boxY, bool hovered);

    bool checked;
    bool selected;
};
