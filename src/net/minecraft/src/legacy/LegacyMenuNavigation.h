#pragma once

#include <vector>

#include "java/Type.h"

class GuiButton;

int_t legacyFirstSelectableButton(const std::vector<GuiButton *> &buttons);
int_t legacyNextSelectableButton(const std::vector<GuiButton *> &buttons, int_t currentIndex, int_t direction);
int_t legacyHoveredSelectableButton(const std::vector<GuiButton *> &buttons, int_t mouseX, int_t mouseY);
void legacyApplyMenuSelection(const std::vector<GuiButton *> &buttons, int_t selectedIndex);
void legacyMoveMenuCursorToSelection(const std::vector<GuiButton *> &buttons, int_t selectedIndex);
