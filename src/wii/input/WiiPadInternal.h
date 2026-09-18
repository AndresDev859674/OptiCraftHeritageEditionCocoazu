#pragma once
#ifdef WII_PLATFORM

#include <cmath>
#include <gccore.h>

#include "wii/input/WiiPadState.h"

namespace WiiPadInternal
{

enum VirtualKey
{
	VK_FORWARD = 0,
	VK_BACK,
	VK_LEFT,
	VK_RIGHT,
	VK_JUMP,
	VK_SNEAK,
	VK_INVENTORY,
	VK_DROP,
	VK_ESCAPE,
	VK_DEBUG,
	VK_THIRDPERSON,
	VK_ARROW_UP,
	VK_ARROW_DOWN,
	VK_ARROW_LEFT,
	VK_ARROW_RIGHT,
	VK_COUNT
};

constexpr u32 VM_ATTACK = 1u << 0;
constexpr u32 VM_USE = 1u << 1;

struct FrameState
{
	u32 keys = 0;
	u32 mouse = 0;
	u32 textInputHeld = 0;
	float moveX = 0.0f;
	float moveY = 0.0f;
	float menuScrollY = 0.0f;
	int wheel = 0;
	bool alternativeControls = false;
	// Whether a GuiScreen is open this frame, and whether that screen asked to be
	// driven by directional navigation instead of a pointer. Supplied by the game
	// through wiiPadPoll(); nothing in the input layer can work these out itself.
	bool inMenu = false;
	bool specializedMenuNavigation = false;
	// Set by WiiRemote when a Classic Controller is the attached expansion; the
	// pad cursor mode (WiiPadState) is only for pads with a face-button layout,
	// a bare Wiimote keeps the directional path.
	bool classicAttached = false;
	float stickDeadzone = 0.20f;
	WiiTextInputSnapshot textInputSnapshot = {};
	WiiStickSnapshot stickSnapshot = {};
	bool menuPadActivity = false;
	bool menuPointerActivity = false;

	void setKey(VirtualKey key, bool down)
	{
		if (down)
			keys |= 1u << key;
	}

	void setMouse(u32 bit, bool down)
	{
		if (down)
			mouse |= bit;
	}

	float applyDeadzone(float value) const
	{
		if (value > -stickDeadzone && value < stickDeadzone)
			return 0.0f;

		const float sign = value < 0.0f ? -1.0f : 1.0f;
		float output = sign * ((std::fabs(value) - stickDeadzone) / (1.0f - stickDeadzone));
		if (output > 1.0f)
			output = 1.0f;
		if (output < -1.0f)
			output = -1.0f;
		return output;
	}
};

} // namespace WiiPadInternal

#endif // WII_PLATFORM
