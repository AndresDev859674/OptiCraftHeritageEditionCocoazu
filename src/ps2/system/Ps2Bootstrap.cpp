#ifdef PS2_PLATFORM

#include "platform/Log.h"
#include "ps2/system/Ps2Bootstrap.h"

#include "ps2/boot/Ps2BootScreen.h"
#include "ps2/input/Ps2Input.h"
#include "ps2/render/Ps2Graphics.h"
#include "ps2/storage/Ps2Storage.h"
#include "ps2/storage/assets/Ps2Assets.h"
#include "ps2/storage/save/Ps2SaveSetup.h"
#include "ps2/system/Ps2EarlyCrash.h"
#include "ps2/system/Ps2Iop.h"
#include "ps2/system/Ps2LibcLocks.h"
#include "ps2/system/Ps2ThreadPriority.h"
#include "platform/storage/PathUtils.h"

#include <kernel.h>

#include <cstdio>
#include <string>

extern "C" void ps2_dbg_init_memory();

namespace Ps2Bootstrap
{

bool initialize(int argc, char** argv)
{
    // The EE kernel has no time slicing and starts main at priority 0, and the
    // frame loop busy-waits on vsync (gsKit_vsync_wait), so at 0 main never
    // yields to worker threads except while blocked in an IOP RPC. The music
    // stream thread was starving on exactly that: it fed audsrv only in those
    // windows and the SPU2 looped whatever it had. Lower main so workers can
    // preempt it.
    ChangeThreadPriority(GetThreadId(), Ps2ThreadPriority::kMain);

    Ps2LibcLocks::initialize();
    // Before anything that can throw. Until this is installed an escaped
    // exception aborts silently, which on this console is indistinguishable
    // from a freeze.
    Ps2EarlyCrash::install();
    ps2_dbg_init_memory();
    MC_LOG_INFO("platform", "[PS2] main() reached\n");

    Ps2Iop::initFileServices();
    MC_LOG_DEBUG("ps2.boot", "[PS2] bootstrap: IOP file services ready\n");

    Ps2Graphics::initialize();
    MC_LOG_DEBUG("ps2.boot", "[PS2] bootstrap: graphics and VU ready\n");
    MC_LOG_INFO("platform", "[PS2] graphics OK - %dx%d\n", Ps2Graphics::width(), Ps2Graphics::height());

    Ps2Input::initialize();
    Ps2Input::waitUntilReady();
    MC_LOG_DEBUG("ps2.boot", "[PS2] pad init done\n");

    Ps2Assets::init(argc, argv);
    if (!Ps2Assets::available())
    {
        MC_LOG_ERROR("platform", "[PS2] FATAL: game data not found. Place data/ beside the ELF\n");
        MC_LOG_INFO("platform", "[PS2]        or under a supported fallback install root.\n");
        MC_LOG_DEBUG("ps2.boot", "[PS2] FATAL: no data/ directory on any readable device\n");
        ps2HaltNoData();
    }

    const std::string installDir = Ps2Assets::installDir();
    // A disc-booted install is read-only. Some CD/DVD IOP drivers do not fail a
    // write attempt cleanly; they leave the SIF RPC call hanging instead of
    // returning an error, which freezes boot before a single log line reaches
    // disk. Never attempt the write there -- go straight to the USB/HDD device
    // a loader typically keeps mounted alongside its disc emulation.
    const bool installIsReadOnlyDisc = Ps2Assets::source() == Ps2AssetLocator::Source::Disc;
    bool logOpened = false;
    if (!installIsReadOnlyDisc)
    {
        logOpened = McLog::openSessionFile(installDir.c_str());
        if (!logOpened)
            MC_LOG_WARN("platform", "[PS2] could not create debug.log in %s\n", installDir.c_str());
    }
    if (!logOpened)
    {
        const std::string massRoot = Ps2Storage::massRoot();
        if (!massRoot.empty() && McLog::openSessionFile(massRoot.c_str()))
            MC_LOG_INFO("platform", "[PS2] debug.log redirected to %s\n", massRoot.c_str());
    }

    MC_LOG_INFO("platform", "[PS2] assets: %s (%s)\n", Ps2Assets::dataDir(), Ps2Assets::sourceName());
    MC_LOG_DEBUG("ps2.boot", "[PS2] assets resolved\n");

    Ps2SaveSetup::selectStorage();
    return true;
}

[[noreturn]] void finishGame()
{
    MC_LOG_ERROR("platform", "[PS2] ERROR: Minecraft::start returned; halting instead of returning to browser\n");
    ps2HaltBlack();
}

} // namespace Ps2Bootstrap

#endif // PS2_PLATFORM
