#ifdef WII_PLATFORM
#include "platform/Log.h"
#include "wii/system/WiiBootstrap.h"

#include <cstdio>

#include "lwjgl/Display.h"
#include "wii/WiiEarlyInit.h"
#include "wii/gx_wii.h"
#include "wii/input/WiiInput.h"
#include "wii/system/WiiConsole.h"

namespace
{

bool showMissingAssetsScreen()
{
	wiiEnsureEarlyVideo();
	WiiConsole::write("\x1b[2J\x1b[H");
	WiiConsole::write("\n\n");
	WiiConsole::write("                 Assets couldn't be loaded.\n\n");
	WiiConsole::write("       Make sure the game files are located in:\n\n");
	WiiConsole::write("             Wii SD: sd:/apps/OptiCraft\n");
	WiiConsole::write("           Wii USB: usb:/apps/OptiCraft\n");
	WiiConsole::write("      PS2 USB: mass:/OptiCraftHeritage\n\n");
	WiiConsole::write("                 Press HOME to exit.\n");
	std::fflush(stdout);

	WiiInput::waitForHome();
	return false;
}

} // namespace

namespace WiiBootstrap
{

bool initialize()
{
	MC_LOG_INFO("wii", "main() entered\n");

	if (!wiiEnsureStorage())
		return showMissingAssetsScreen();

	if (!wiiHasGameData())
		return showMissingAssetsScreen();

	MC_LOG_INFO("wii", "app dir: %s\n", wiiGetAppDir());
	MC_LOG_INFO("wii", "starting GX/input...\n");
	lwjgl::Display::create();
	return true;
}

void shutdown()
{
	WiiInput::shutdown();
	wiigl_shutdown();
}

}
#endif
