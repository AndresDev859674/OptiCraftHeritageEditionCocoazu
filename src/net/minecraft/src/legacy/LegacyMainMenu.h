#pragma once

#include <vector>

#include "java/Type.h"

class GuiButton;

void legacyCreateMainMenuButtons(std::vector<GuiButton *> &controlList, GuiButton *&multiplayerButton,
    int_t screenWidth, int_t screenHeight, bool hideQuitButton);
