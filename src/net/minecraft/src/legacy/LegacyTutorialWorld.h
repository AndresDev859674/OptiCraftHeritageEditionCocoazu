#pragma once

#include <string>

class Minecraft;

namespace LegacyTutorialWorld
{
const std::string &saveDirectoryName();
const std::string &displayName();
bool ensureInstalled(Minecraft *mc, std::string &errorMessage);
bool play(Minecraft *mc, std::string &errorMessage);
}
