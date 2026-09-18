#pragma once

#ifdef PS2_PLATFORM

#include "ps2/storage/assets/Ps2AssetLocator.h"

#include <string>

namespace Ps2Assets
{
    using Source = Ps2AssetLocator::Source;

    void init(int argc = 0, char* argv[] = nullptr);

    bool available();
    const char* dataDir();
    Source source();
    const char* sourceName();

    // Directory holding data/, i.e. the folder the game was installed into.
    // Empty until the assets resolve. Falls back to the bare device root for a
    // device whose paths have no slash after the colon ("host:data"), where
    // taking the parent would otherwise lose the device entirely.
    std::string installDir();

    std::string assetDir();
    std::string resourcesDir();
    std::string resolve(const std::string& key);
    std::string resolveExisting(const std::string& key);

    long fileSize(const std::string& path);
    unsigned char* loadAsset(const std::string& key, unsigned int* outSize);
    unsigned char* loadFile(const std::string& path, unsigned int* outSize);
}

#endif // PS2_PLATFORM
