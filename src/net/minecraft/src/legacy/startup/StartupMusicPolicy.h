#pragma once

#include <cctype>
#include <string>

namespace LegacyStartup
{

inline bool shouldStartCalmMusic(bool legacyUi, float musicVolume)
{
    return legacyUi && musicVolume > 0.0f;
}

inline bool isCalmTrackFilename(const std::string &filename)
{
    if (filename.size() < 7)
        return false;

    const char prefix[] = "calm";
    for (std::size_t i = 0; i < 4; ++i)
    {
        if (std::tolower(static_cast<unsigned char>(filename[i])) != prefix[i])
            return false;
    }

    std::size_t index = 4;
    const std::size_t digitStart = index;
    while (index < filename.size() && std::isdigit(static_cast<unsigned char>(filename[index])))
        ++index;

    return index > digitStart && index < filename.size() && filename[index] == '.';
}

} // namespace LegacyStartup
