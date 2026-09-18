#pragma once

#include <string>

namespace Ps2MusicStream
{
bool start(const std::string &path, float volume);
void stop();
bool active();
void setVolume(float volume);
}
