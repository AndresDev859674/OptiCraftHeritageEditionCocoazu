#pragma once

#include <cctype>
#include <string>

namespace LegacyStartup
{

inline bool shouldStartCalmMusic(bool legacyUi, float musicVolume)
{
    return legacyUi && musicVolume > 0.0f;
}

inline bool isStartupMusicTrackFilename(const std::string &filename)
{
    if (filename.size() < 8)
        return false;

    const auto startsWith = [&filename](const char *prefix, std::size_t length)
    {
        if (filename.size() < length)
            return false;
        for (std::size_t i = 0; i < length; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(filename[i])) != prefix[i])
                return false;
        }
        return true;
    };

    const bool isCalm = startsWith("calm", 4);
    const bool isHal = startsWith("hal", 3);
    const bool isNuance = startsWith("nuance", 6);
    const bool isPiano = startsWith("piano", 5);
    if (!isCalm && !isHal && !isNuance && !isPiano)
        return false;

    const std::size_t prefixLength = isCalm ? 4 : isHal ? 3 : isNuance ? 6 : 5;
    std::size_t index = prefixLength;
    const std::size_t digitStart = index;
    while (index < filename.size() && std::isdigit(static_cast<unsigned char>(filename[index])))
        ++index;

    if (index == digitStart || index + 4 != filename.size() ||
        std::tolower(static_cast<unsigned char>(filename[index + 1])) != 'o' ||
        std::tolower(static_cast<unsigned char>(filename[index + 2])) != 'g' ||
        std::tolower(static_cast<unsigned char>(filename[index + 3])) != 'g')
        return false;

    return true;
}

} // namespace LegacyStartup
