#pragma once

#include <string>

namespace PcMusicStream
{
bool start(const std::string &path);
void stop();
bool active();
void mix(float *stereoOutput, int frames, float volume);
}
