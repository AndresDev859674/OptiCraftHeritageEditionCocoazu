#ifdef WII_PLATFORM

#include "wii/input/WiiButtonBindings.h"

#include <gccore.h>
#include <wiiuse/wpad.h>

namespace WiiButtonBindings
{
namespace
{
	// Classic reproduces what WiiRemote.cpp used to hardcode before per-family
	// rebinding existed, plus ThirdPerson on D-pad up, which was unbound there.
	// GameCube puts Sneak on D-pad down and ThirdPerson on D-pad up (Sneak used
	// to be B, ThirdPerson D-pad down); the debug overlay moved to Z, see
	// WiiGameCubePad.cpp.
	//
	// The Wiimote layout keeps Sneak on A and Inventory on Minus, and rotates
	// the other three so the hand doing the aiming is not also the hand doing
	// the digging: Jump moves to Nunchuk C, Attack to Nunchuk Z, and Use to B.
	// Both the normal and the alternative scheme use these same four, so they
	// do not have to be relearned when the scheme is switched.
	//
	// Drop sits on 1. It used to be 2, which now cycles the hotbar, and 1 came
	// free when diagnostics became the 1+2 chord (both in WiiRemote.cpp).
	// ThirdPerson is D-pad up in the normal scheme; the alternative scheme uses
	// the D-pad for movement and toggles it with Nunchuk C+Z instead (see
	// WiiRemote.cpp).
	const Snapshot s_defaults = {
		{ PAD_BUTTON_A, PAD_BUTTON_DOWN, PAD_BUTTON_Y, PAD_BUTTON_X, PAD_TRIGGER_R, PAD_TRIGGER_L, PAD_BUTTON_UP },
		{ WPAD_NUNCHUK_BUTTON_C, WPAD_BUTTON_A, WPAD_BUTTON_1, WPAD_BUTTON_MINUS, WPAD_NUNCHUK_BUTTON_Z, WPAD_BUTTON_B, WPAD_BUTTON_UP },
		{ WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B, WPAD_CLASSIC_BUTTON_Y, WPAD_CLASSIC_BUTTON_X,
		  WPAD_CLASSIC_BUTTON_FULL_R, WPAD_CLASSIC_BUTTON_FULL_L, WPAD_CLASSIC_BUTTON_UP },
	};
	Snapshot s_snapshot = s_defaults;
}

void set(const Snapshot &snapshot)
{
	s_snapshot = snapshot;
}

const Snapshot &get()
{
	return s_snapshot;
}

const Snapshot &defaults()
{
	return s_defaults;
}

}

#endif // WII_PLATFORM
