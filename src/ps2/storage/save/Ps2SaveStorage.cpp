#ifdef PS2_PLATFORM

#include "ps2/storage/save/Ps2SaveStorage.h"
#include "ps2/storage/Ps2Storage.h"
#include "ps2/storage/assets/Ps2Assets.h"
#include "platform/storage/PathUtils.h"

namespace
{

struct SaveStorageState
{
    Ps2SaveStorage::Target target = Ps2SaveStorage::Target::Disabled;
    std::string massRoot;
    std::string displayRoot;
};

SaveStorageState& state()
{
    static SaveStorageState value;
    return value;
}

} // namespace

namespace Ps2SaveStorage
{

void setTarget(Target targetValue)
{
    state().target = targetValue;
    state().massRoot.clear();
    state().displayRoot.clear();

    if (targetValue == Target::MassStorage)
    {
        state().massRoot = Ps2Storage::massRoot();
        if (state().massRoot.empty())
            state().massRoot = "mass:/";

        // Saves live inside the install folder, next to data/. Following the
        // resolved install directory rather than a fixed name is what lets the
        // folder be called anything: rename it and the saves inside travel with
        // it, so there is nothing to migrate.
        //
        // Only when the install cannot hold them -- a read-only disc, or a host:
        // launch -- do saves need a home of their own, and then the conventional
        // name on mass storage is the one place both a disc build and a USB
        // build will agree on.
        const bool installIsWritable = Ps2Assets::source() == Ps2Assets::Source::UsbMass ||
                                       Ps2Assets::source() == Ps2Assets::Source::HardDisk;
        const std::string installDir = Ps2Assets::installDir();
        if (installIsWritable && !installDir.empty())
            state().displayRoot = installDir;
        else
            state().displayRoot = PlatformStorage::join(state().massRoot, Ps2AssetLocator::INSTALL_FOLDER);
    }
}

Target target()
{
    return state().target;
}

bool enabled()
{
    return state().target != Target::Disabled;
}

std::string root()
{
    switch (state().target)
    {
        case Target::MemoryCard:
            return "mc0:";
        case Target::MassStorage:
            return state().displayRoot;
        case Target::Disabled:
        default:
            return "nosave:";
    }
}

const char* displayRoot()
{
    switch (state().target)
    {
        case Target::MemoryCard:
            return "mc0:";
        case Target::MassStorage:
            return state().displayRoot.c_str();
        case Target::Disabled:
        default:
            return "(disabled)";
    }
}

} // namespace Ps2SaveStorage

#endif // PS2_PLATFORM
