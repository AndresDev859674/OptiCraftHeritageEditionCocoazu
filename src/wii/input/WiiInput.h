#pragma once
#ifdef WII_PLATFORM

namespace WiiInput
{
// Owns libogc controller lifecycle. Safe to call more than once.
bool initialize(int pointerWidth, int pointerHeight);
void setPointerResolution(int width, int height);
// inMenu is what selects the menu input mapping over the gameplay one, and
// what makes the IR pointer an absolute cursor instead of a turn rate.
// specializedMenuNavigation is the current screen's own request to be driven
// by directional navigation rather than a pointer. Both come from the game --
// see Display_wii.cpp's processMessages().
void poll(bool inMenu, bool specializedMenuNavigation);
void shutdown();

// Crash-safe path: makes the Wiimote stack usable even if normal display/input
// initialization never completed, then blocks until HOME is pressed.
void waitForHome();

bool initialized();
}

#endif
