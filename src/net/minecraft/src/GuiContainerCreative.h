#pragma once

#include "GuiContainer.h"

class EntityPlayer;
class InventoryBasic;

// net.minecraft.src.GuiContainerCreative
class GuiContainerCreative : public GuiContainer
{
public:
    explicit GuiContainerCreative(EntityPlayer *player);

    void updateScreen() override;
    void initGui() override;
    void handleMouseInput() override;
    void drawScreen(int_t mouseX, int_t mouseY, float_t partialTick) override;
    Slot *getControllerNavigationTarget(Slot *selected, int_t dirX, int_t dirY) override;

    static InventoryBasic *getInventory();

protected:
    void handleMouseClick(Slot *slot, int_t slotId, int_t button, bool shift) override;
    void drawGuiContainerForegroundLayer() override;
    void drawGuiContainerBackgroundLayer(float_t partialTick) override;
    void actionPerformed(GuiButton *button) override;

private:
    bool scrollRows(int_t direction);

    static InventoryBasic inventory;
    float_t currentScroll;
    bool isScrolling;
    bool wasClicking;
};
