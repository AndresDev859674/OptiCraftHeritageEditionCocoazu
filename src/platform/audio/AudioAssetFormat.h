#pragma once

#include <cctype>
#include <cstring>
#include <string>

inline bool audioPathHasExtension(const std::string &path, const char *extension)
{
    const std::size_t extensionLength = std::strlen(extension);
    if (path.size() < extensionLength)
        return false;

    const std::size_t offset = path.size() - extensionLength;
    for (std::size_t i = 0; i < extensionLength; ++i)
    {
        const unsigned char pathChar = static_cast<unsigned char>(path[offset + i]);
        const unsigned char extensionChar = static_cast<unsigned char>(extension[i]);
        if (std::tolower(pathChar) != std::tolower(extensionChar))
            return false;
    }
    return true;
}
