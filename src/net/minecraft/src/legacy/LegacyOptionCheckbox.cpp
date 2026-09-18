#include "LegacyOptionCheckbox.h"

#include "LegacyOptionMetrics.h"
#include "LegacyOptionText.h"
#include "LegacyOptionStyle.h"
#include "LegacyUiTexture.h"
#include "LegacyUiTheme.h"
#include "net/minecraft/src/FontRenderer.h"
#include "net/minecraft/src/Minecraft.h"

namespace
{
// One cache per sprite, shared by every checkbox on every Legacy screen: the
// three lookups happen once per texture pack instead of once per widget.
LegacyUiTexture g_tickBox("/legacy/tickbox.png");
LegacyUiTexture g_tickBoxHovered("/legacy/tickbox_hovered.png");
LegacyUiTexture g_tick("/legacy/tick.png");
}

LegacyOptionCheckbox::LegacyOptionCheckbox(int_t id, int_t x, int_t y, int_t width, int_t height,
    const std::string &label, bool checkedValue)
    : GuiButton(id, x, y, width, height, label), checked(checkedValue), selected(false)
{
}

void LegacyOptionCheckbox::setChecked(bool checkedValue)
{
    checked = checkedValue;
}

bool LegacyOptionCheckbox::isChecked() const
{
    return checked;
}

void LegacyOptionCheckbox::drawProceduralBox(int_t boxX, int_t boxY, int_t boxSize, bool hovered)
{
    const LegacyUiTheme &theme = legacyUiTheme();
    drawRect(boxX, boxY, boxX + boxSize, boxY + boxSize, theme.checkboxBorderColor);
    drawRect(boxX + 1, boxY + 1, boxX + boxSize - 1, boxY + boxSize - 1,
        hovered ? theme.checkboxHoverFillColor : theme.checkboxFillColor);
    drawRect(boxX + 2, boxY + 2, boxX + boxSize - 2, boxY + boxSize - 2,
        theme.checkboxInnerColor);
}

void LegacyOptionCheckbox::drawProceduralTick(int_t boxX, int_t boxY, bool hovered)
{
    const LegacyUiTheme &theme = legacyUiTheme();
    const int_t check = hovered ? theme.checkboxTickHoverColor : theme.checkboxTickColor;
    // Compact 4J-style pixel tick, scaled to the 10 px native box.
    drawRect(boxX + 2, boxY + 4, boxX + 4, boxY + 6, check);
    drawRect(boxX + 4, boxY + 5, boxX + 6, boxY + 8, check);
    drawRect(boxX + 6, boxY + 2, boxX + 8, boxY + 7, check);
}

void LegacyOptionCheckbox::drawButton(Minecraft *mc, int_t mouseX, int_t mouseY)
{
    if (!enabled2 || mc == nullptr || mc->fontRenderer == nullptr)
        return;

    const LegacyUiTheme &theme = legacyUiTheme();
    const bool hovered = selected || (enabled && mouseX >= xPosition && mouseY >= yPosition &&
        mouseX < xPosition + width && mouseY < yPosition + height);
    const int_t boxSize = theme.checkboxSize;
    const int_t boxX = xPosition + 1;
    const int_t boxY = yPosition + (height - boxSize) / 2;

    // The sprite is drawn at its native size centred on the layout box, so the
    // label keeps the same x whether the Legacy art is installed or not.
    const int_t spriteSize = theme.checkboxTextureSize;
    const int_t spriteX = boxX - (spriteSize - boxSize) / 2;
    const int_t spriteY = boxY - (spriteSize - boxSize) / 2;
    const int_t boxTexture = (hovered ? g_tickBoxHovered : g_tickBox).resolve(mc->renderEngine);

    if (boxTexture >= 0)
        legacyDrawUiTexture(boxTexture, spriteX, spriteY, spriteSize, spriteSize, zLevel);
    else
        drawProceduralBox(boxX, boxY, boxSize, hovered);

    if (checked)
    {
        const int_t tickTexture = g_tick.resolve(mc->renderEngine);
        if (tickTexture >= 0)
            legacyDrawUiTexture(tickTexture, spriteX + theme.checkboxTickOffsetX,
                spriteY + theme.checkboxTickOffsetY, theme.checkboxTickWidth,
                theme.checkboxTickHeight, zLevel);
        else
            drawProceduralTick(boxX, boxY, hovered);
    }

    const int_t textColor = !enabled ? theme.disabledTextColor
        : (hovered ? theme.selectedTextColor : theme.normalTextColor);
    // Legacy4J offsets unfocused tickbox text very slightly; a full-pixel offset
    // is too large at this port's 8px font, so keep the glyph at the native row
    // baseline and let the restrained lower-right edge provide the same depth.
    legacyDrawOptionText(mc->fontRenderer, displayString, boxX + boxSize + 2,
        legacyOptionTextY(yPosition, height), textColor);
}

void LegacyOptionCheckbox::setKeyboardSelected(bool selectedValue)
{
    selected = selectedValue;
}
