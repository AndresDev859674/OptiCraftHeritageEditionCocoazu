#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include "ps2/storage/assets/Ps2Assets.h"
#include "ps2/storage/Ps2Storage.h"
#include "platform/storage/AssetPak.h"
#include "platform/storage/PathUtils.h"
#include "platform/storage/PosixFileSystem.h"

#include <cstdio>
#include <string>

namespace
{

struct AssetState
{
    bool resolutionAttempted = false;
    bool available = false;
    std::string dataDir;
    Ps2Assets::Source source = Ps2Assets::Source::Unknown;
};

AssetState& state()
{
    static AssetState value;
    return value;
}

void ensureResolved()
{
    if (state().resolutionAttempted)
        return;

    state().resolutionAttempted = true;
    Ps2AssetLocator::Result result;
    if (!Ps2AssetLocator::resolve(result))
        return;

    state().available = true;
    state().dataDir = result.dataRoot;
    state().source = result.source;

    // The locator may have accepted the root on the loose probe file while a
    // pak sits beside data/ as well; mount it so the lookups below prefer it.
    // A root accepted through the pak is already mounted and this is a no-op.
    // Not before the IOP file services are up (see candidateHasPak): the
    // handle is kept for the session, and init() re-resolves after them.
    if (Ps2Storage::fileIoReady())
        AssetPak::mountFrom(Ps2Assets::installDir());
}

unsigned char* loadResolvedPath(const std::string& path, unsigned int* outSize)
{
    unsigned char* data = Ps2Storage::readWholeFile(path, outSize, 64);
    if (!data)
        MC_LOG_ERROR("assets", "[PS2][assets] failed to read %s\n", path.c_str());
    return data;
}

} // namespace

namespace Ps2Assets
{

void init(int argc, char* argv[])
{
    Ps2AssetLocator::init(argc, argv);
    state().resolutionAttempted = false;
    state().available = false;
    state().dataDir.clear();
    state().source = Source::Unknown;
    ensureResolved();
}

bool available()
{
    ensureResolved();
    return state().available;
}

const char* dataDir()
{
    ensureResolved();
    return state().dataDir.c_str();
}

Source source()
{
    ensureResolved();
    return state().source;
}

const char* sourceName()
{
    ensureResolved();
    return Ps2AssetLocator::sourceName(state().source);
}

std::string installDir()
{
    ensureResolved();
    if (state().dataDir.empty())
        return std::string();

    const std::string dataDir = PlatformStorage::normalizeSlashes(state().dataDir);
    std::string parent = PlatformStorage::parent(dataDir);
    // "host:data" has no slash, unlike "mass:/data", so parent() returns
    // nothing. Keep the device root rather than losing it.
    if (parent.empty())
    {
        const std::size_t colon = dataDir.find(':');
        if (colon != std::string::npos)
            parent = dataDir.substr(0, colon + 1);
    }
    return parent;
}

std::string assetDir()
{
    ensureResolved();
    return Ps2AssetLocator::resolveDirectory(state().dataDir, state().source, "assets");
}

std::string resourcesDir()
{
    ensureResolved();
    return Ps2AssetLocator::resolveDirectory(state().dataDir, state().source, "resources");
}

std::string resolve(const std::string& key)
{
    ensureResolved();
    return PlatformStorage::join(state().dataDir, key);
}

std::string resolveExisting(const std::string& key)
{
    ensureResolved();
    // Pak entry first: keys are spelled the same way ("assets/gui/items.png").
    if (AssetPak::mounted() && AssetPak::exists(key))
        return AssetPak::makePath(key);
    return Ps2AssetLocator::resolveFile(state().dataDir, state().source, key);
}

long fileSize(const std::string& path)
{
    ensureResolved();
    if (AssetPak::isPakPath(path))
        return AssetPak::size(AssetPak::keyOf(path));
    return static_cast<long>(PlatformStorage::fileSize(path));
}

unsigned char* loadAsset(const std::string& key, unsigned int* outSize)
{
    const std::string path = resolveExisting(key);
    if (path.empty())
    {
        if (outSize)
            *outSize = 0;
        MC_LOG_INFO("assets", "[PS2][assets] missing %s\n", key.c_str());
        return nullptr;
    }
    if (AssetPak::isPakPath(path))
        return AssetPak::load(AssetPak::keyOf(path), outSize);
    return loadResolvedPath(path, outSize);
}

unsigned char* loadFile(const std::string& path, unsigned int* outSize)
{
    ensureResolved();
    if (AssetPak::isPakPath(path))
        return AssetPak::load(AssetPak::keyOf(path), outSize);
    return loadResolvedPath(path, outSize);
}

} // namespace Ps2Assets

#endif // PS2_PLATFORM
