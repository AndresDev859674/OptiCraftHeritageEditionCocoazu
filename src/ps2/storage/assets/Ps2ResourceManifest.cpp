#ifdef PS2_PLATFORM

#include "ps2/storage/assets/Ps2ResourceManifest.h"
#include "ps2/storage/assets/Ps2Assets.h"
#include "platform/storage/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace
{

struct ManifestState
{
    bool loaded = false;
    std::string root;                  // resources root: lowercased, '/'-separated, no trailing slash
    std::vector<std::string> entries;  // manifest lines: lowercased, '/'-separated, relative to root
};

ManifestState& state()
{
    static ManifestState value;
    return value;
}

std::string toLower(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string stripTrailingSlash(std::string value)
{
    while (!value.empty() && value.back() == '/')
        value.pop_back();
    return value;
}

void ensureLoaded()
{
    if (state().loaded)
        return;
    state().loaded = true;

    state().root = toLower(stripTrailingSlash(PlatformStorage::normalizeSlashes(Ps2Assets::resourcesDir())));

    unsigned int size = 0;
    unsigned char* data = Ps2Assets::loadAsset("resources.manifest", &size);
    if (!data)
        return;

    std::string text(reinterpret_cast<char*>(data), size);
    std::free(data);

    std::size_t pos = 0;
    while (pos <= text.size())
    {
        const std::size_t newline = text.find('\n', pos);
        std::string line = text.substr(pos, newline == std::string::npos ? std::string::npos : newline - pos);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (!line.empty())
        {
            std::replace(line.begin(), line.end(), '\\', '/');
            state().entries.push_back(toLower(line));
        }
        if (newline == std::string::npos)
            break;
        pos = newline + 1;
    }
}

// Manifest-relative path for discPath ("" for the resources root itself).
// False if discPath isn't under the resources root at all.
bool relativize(const std::string& discPath, std::string& outRelative)
{
    ensureLoaded();
    if (state().root.empty())
        return false;

    const std::string normalized = toLower(stripTrailingSlash(PlatformStorage::normalizeSlashes(discPath)));
    if (normalized == state().root)
    {
        outRelative.clear();
        return true;
    }

    const std::string prefix = state().root + "/";
    if (normalized.compare(0, prefix.size(), prefix) != 0)
        return false;

    outRelative = normalized.substr(prefix.size());
    return true;
}

} // namespace

namespace Ps2ResourceManifest
{

bool covers(const std::string& discPath)
{
    std::string relative;
    return relativize(discPath, relative);
}

bool isDirectory(const std::string& discPath)
{
    std::string relative;
    if (!relativize(discPath, relative))
        return false;
    if (relative.empty())
        return !state().entries.empty();

    const std::string prefix = relative + "/";
    for (const std::string& entry : state().entries)
    {
        if (entry.compare(0, prefix.size(), prefix) == 0)
            return true;
    }
    return false;
}

bool listChildren(const std::string& discPath, std::vector<std::string>& out)
{
    out.clear();
    std::string relative;
    if (!relativize(discPath, relative))
        return false;

    const std::string prefix = relative.empty() ? std::string() : relative + "/";
    for (const std::string& entry : state().entries)
    {
        if (entry.compare(0, prefix.size(), prefix) != 0)
            continue;
        const std::string remainder = entry.substr(prefix.size());
        const std::size_t slash = remainder.find('/');
        const std::string child = slash == std::string::npos ? remainder : remainder.substr(0, slash);
        if (child.empty())
            continue;
        if (std::find(out.begin(), out.end(), child) == out.end())
            out.push_back(child);
    }
    return true;
}

} // namespace Ps2ResourceManifest

#endif // PS2_PLATFORM
