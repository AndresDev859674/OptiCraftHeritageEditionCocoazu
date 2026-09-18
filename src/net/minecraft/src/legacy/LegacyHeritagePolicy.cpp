#include "LegacyHeritagePolicy.h"

std::string sanitizeHeritagePlayerName(const std::string &name)
{
    std::string result;
    for (char c : name)
    {
        const bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_';
        if (valid && result.size() < 16)
            result.push_back(c);
    }
    return result.empty() ? "Player" : result;
}
