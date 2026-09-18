#ifdef WII_PLATFORM

#include "lwjgl/Keyboard.h"
#include "lwjgl/Mouse.h"
#include "platform/Input.h"
#include "platform/ConsoleInputClock.h"
#include "platform/PlatformKeyBindings.h"
#include "wii/input/WiiContainerCursorPolicy.h"
#include "wii/input/WiiGameCubePad.h"
#include "wii/input/WiiInputDebug.h"
#include "wii/input/WiiPadInternal.h"
#include "wii/input/WiiPadPoll.h"
#include "wii/input/WiiPadState.h"
#include "wii/input/WiiPointer.h"
#include "wii/input/WiiRemote.h"

namespace
{
WiiTextInputSnapshot g_textInputSnapshot = {};
WiiStickSnapshot g_stickSnapshot = {};
u32 g_textInputLastHeld = 0;
u32 g_latchedTextPressed = 0;
bool g_alternativeControls = false;
// Latched once per wiiPadPoll() so every helper below sees the same answer for
// the whole frame, rather than each one re-deriving it from a different proxy.
bool g_inMenu = false;
bool g_specializedMenuNavigation = false;
// A GameCube or Classic pad owning a Java UI screen drives a PS2-style cursor.
// Container screens arbitrate this dynamically: the left stick hands control to
// the pointer while the D-pad hands it back to slot navigation.
bool g_padCursorActive = false;

// Menu and gameplay give the same physical button different meanings, so a
// button still held across the boundary would be delivered as a fresh press
// under the new mapping. That is what made joining a world bounce straight back
// to the title: the press that confirmed the menu entry arrived again as a
// gameplay button the moment the world opened. Anything down at the transition
// is ignored until it is physically released. The PS2 front end solves the same
// problem in Ps2InputMapper::update() with releaseGameplayKeys().
bool g_previousInMenu = false;
u32 g_suppressedKeys = 0;
u32 g_suppressedMouse = 0;
float g_stickDeadzone = 0.20f;
u32 g_previousKeys = 0;
u32 g_previousMouse = 0;

constexpr float MENU_NAV_ENTER_THRESHOLD = 0.60f;
constexpr float MENU_NAV_RELEASE_THRESHOLD = 0.35f;
constexpr float MENU_SCROLL_THRESHOLD = 0.18f;
constexpr int MENU_NAV_REPEAT_DELAY_MS = 300;
constexpr int MENU_NAV_REPEAT_INTERVAL_MS = 110;
constexpr int MENU_SCROLL_MAX_INTERVAL_MS = 200;
constexpr int MENU_SCROLL_INTERVAL_RANGE_MS = 160;
constexpr u32 WII_MENU_DIRECTION_MASK =
	WII_TEXT_LEFT | WII_TEXT_RIGHT | WII_TEXT_UP | WII_TEXT_DOWN;

u32 g_menuAnalogDirection = 0;
u32 g_menuRepeatHeld = 0;
int g_menuRepeatAtMs = 0;
int g_menuScrollDirection = 0;
int g_menuScrollAtMs = 0;

enum class WiiMenuInputOwner
{
	RemotePointer,
	RemotePad,
	GameCubePad,
};

WiiMenuInputOwner g_menuInputOwner = WiiMenuInputOwner::RemotePointer;

// The first 8 entries (Forward..Drop) mirror GameSettings' live keyBindings so
// remapping an action in the Controls menu actually changes what the pad
// sends, instead of the pad always synthesizing the vanilla defaults. The
// rest (menu/debug/thirdperson/on-screen-nav) aren't player-rebindable.
int kVirtualKeyCodes[WiiPadInternal::VK_COUNT] = {
	lwjgl::Keyboard::KEY_W,
	lwjgl::Keyboard::KEY_S,
	lwjgl::Keyboard::KEY_A,
	lwjgl::Keyboard::KEY_D,
	lwjgl::Keyboard::KEY_SPACE,
	lwjgl::Keyboard::KEY_LSHIFT,
	lwjgl::Keyboard::KEY_E,
	lwjgl::Keyboard::KEY_Q,
	lwjgl::Keyboard::KEY_ESCAPE,
	lwjgl::Keyboard::KEY_F3,
	lwjgl::Keyboard::KEY_F5,
	lwjgl::Keyboard::KEY_UP,
	lwjgl::Keyboard::KEY_DOWN,
	lwjgl::Keyboard::KEY_LEFT,
	lwjgl::Keyboard::KEY_RIGHT,
};
unsigned g_bindingsVersion = 0;

void syncVirtualKeyCodes()
{
	const unsigned version = PlatformKeyBindings::version();
	if (version == g_bindingsVersion)
		return;

	// A rebind mid-hold must not leave the outgoing key stuck down, nor
	// silently eat the button because its VK bit looked unchanged to
	// flushKeysAndButtons (it XORs against the *bit*, not the key code).
	static const WiiPadInternal::VirtualKey kRemappable[] = {
		WiiPadInternal::VK_FORWARD, WiiPadInternal::VK_BACK,
		WiiPadInternal::VK_LEFT, WiiPadInternal::VK_RIGHT,
		WiiPadInternal::VK_JUMP, WiiPadInternal::VK_SNEAK,
		WiiPadInternal::VK_INVENTORY, WiiPadInternal::VK_DROP,
	};
	for (WiiPadInternal::VirtualKey vk : kRemappable)
	{
		const u32 bit = 1u << vk;
		if (g_previousKeys & bit)
		{
			lwjgl::Keyboard::detail::pushKey(kVirtualKeyCodes[vk], false);
			g_previousKeys &= ~bit;
		}
	}

	const PlatformKeyBindings::Snapshot &bindings = PlatformKeyBindings::get();
	kVirtualKeyCodes[WiiPadInternal::VK_FORWARD] = bindings.forward;
	kVirtualKeyCodes[WiiPadInternal::VK_BACK] = bindings.back;
	kVirtualKeyCodes[WiiPadInternal::VK_LEFT] = bindings.left;
	kVirtualKeyCodes[WiiPadInternal::VK_RIGHT] = bindings.right;
	kVirtualKeyCodes[WiiPadInternal::VK_JUMP] = bindings.jump;
	kVirtualKeyCodes[WiiPadInternal::VK_SNEAK] = bindings.sneak;
	kVirtualKeyCodes[WiiPadInternal::VK_INVENTORY] = bindings.inventory;
	kVirtualKeyCodes[WiiPadInternal::VK_DROP] = bindings.drop;
	g_bindingsVersion = version;
}

float absoluteValue(float value)
{
	return value < 0.0f ? -value : value;
}

u32 menuAnalogDirection(float x, float y)
{
	const float absX = absoluteValue(x);
	const float absY = absoluteValue(y);
	if (absX < MENU_NAV_ENTER_THRESHOLD && absY < MENU_NAV_ENTER_THRESHOLD)
		return 0;
	if (absY >= absX)
		return y > 0.0f ? WII_TEXT_UP : WII_TEXT_DOWN;
	return x < 0.0f ? WII_TEXT_LEFT : WII_TEXT_RIGHT;
}

float menuAnalogActiveAxis(const WiiPadInternal::FrameState& state, u32 direction)
{
	if ((direction & (WII_TEXT_UP | WII_TEXT_DOWN)) != 0)
		return absoluteValue(state.moveY);
	return absoluteValue(state.moveX);
}

void resetMenuNavigation()
{
	g_menuAnalogDirection = 0;
	g_menuRepeatHeld = 0;
	g_menuRepeatAtMs = 0;
}

int updateGameCubeMenuScroll(float y, bool active)
{
	const float magnitude = absoluteValue(y);
	if (!active || magnitude < MENU_SCROLL_THRESHOLD)
	{
		g_menuScrollDirection = 0;
		g_menuScrollAtMs = 0;
		return 0;
	}

	const int direction = y > 0.0f ? 1 : -1;
	const int now = consoleInputNowMs();
	const float clampedMagnitude = magnitude > 1.0f ? 1.0f : magnitude;
	const int interval = MENU_SCROLL_MAX_INTERVAL_MS -
		(int)(MENU_SCROLL_INTERVAL_RANGE_MS * clampedMagnitude);

	if (direction != g_menuScrollDirection)
	{
		g_menuScrollDirection = direction;
		g_menuScrollAtMs = now + interval;
		return direction;
	}

	if (g_menuScrollAtMs != 0 && now >= g_menuScrollAtMs)
	{
		g_menuScrollAtMs = now + interval;
		return direction;
	}

	return 0;
}

bool menuPadNavigationHeld(const WiiPadInternal::FrameState& state)
{
	if ((state.textInputHeld & WII_MENU_DIRECTION_MASK) != 0)
		return true;
	return absoluteValue(state.moveX) > MENU_NAV_RELEASE_THRESHOLD ||
		absoluteValue(state.moveY) > MENU_NAV_RELEASE_THRESHOLD;
}

u32 updateMenuNavigation(WiiPadInternal::FrameState& state, bool active, bool synthesizeRepeat)
{
	if (!active)
	{
		resetMenuNavigation();
		return 0;
	}

	if (g_menuAnalogDirection != 0 &&
		menuAnalogActiveAxis(state, g_menuAnalogDirection) <= MENU_NAV_RELEASE_THRESHOLD)
	{
		g_menuAnalogDirection = 0;
	}
	if (g_menuAnalogDirection == 0)
		g_menuAnalogDirection = menuAnalogDirection(state.moveX, state.moveY);

	state.textInputHeld |= g_menuAnalogDirection;
	const u32 heldDirections = state.textInputHeld & WII_MENU_DIRECTION_MASK;
	if (!synthesizeRepeat)
	{
		g_menuRepeatHeld = 0;
		g_menuRepeatAtMs = 0;
		return 0;
	}

	if (heldDirections == 0)
	{
		g_menuRepeatHeld = 0;
		g_menuRepeatAtMs = 0;
		return 0;
	}

	const int now = consoleInputNowMs();
	if (heldDirections != g_menuRepeatHeld)
	{
		g_menuRepeatHeld = heldDirections;
		g_menuRepeatAtMs = now + MENU_NAV_REPEAT_DELAY_MS;
		return 0;
	}
	if (g_menuRepeatAtMs != 0 && now >= g_menuRepeatAtMs)
	{
		g_menuRepeatAtMs = now + MENU_NAV_REPEAT_INTERVAL_MS;
		return heldDirections;
	}
	return 0;
}

void updateTextInputSnapshot(u32 held, u32 repeatedPressed)
{
	const u32 pressed = held & ~g_textInputLastHeld;
	g_textInputSnapshot.held = held;
	g_textInputSnapshot.pressed = pressed;
	if (lwjgl::Mouse::isGrabbed())
	{
		g_latchedTextPressed = 0;
	}
	else
	{
		g_latchedTextPressed |= pressed;
		g_latchedTextPressed |= repeatedPressed & WII_MENU_DIRECTION_MASK;
	}
	g_textInputLastHeld = held;
}

void configureFrameState(WiiPadInternal::FrameState& state)
{
	state.alternativeControls = g_alternativeControls;
	state.inMenu = g_inMenu;
	state.specializedMenuNavigation = g_specializedMenuNavigation;
	state.stickDeadzone = g_stickDeadzone;
}

void mergeFrameState(WiiPadInternal::FrameState& target, const WiiPadInternal::FrameState& source)
{
	target.keys |= source.keys;
	target.mouse |= source.mouse;
	target.textInputHeld |= source.textInputHeld;
	target.moveX += source.moveX;
	target.moveY += source.moveY;
	target.menuScrollY += source.menuScrollY;
	target.wheel += source.wheel;
	if (source.textInputSnapshot.irWidth > 0)
		target.textInputSnapshot = source.textInputSnapshot;
	if (source.stickSnapshot.connected)
		target.stickSnapshot = source.stickSnapshot;
}

void updateMenuInputOwner(std::uint32_t gameCubeConnected,
	const WiiPadInternal::FrameState& gameCubeState, const WiiPadInternal::FrameState& remoteState)
{
	WiiMenuInputOwner nextOwner = g_menuInputOwner;
	const bool gameCubePadActivity = gameCubeState.menuPadActivity ||
		menuAnalogDirection(gameCubeState.moveX, gameCubeState.moveY) != 0;
	const bool remotePadActivity = remoteState.menuPadActivity ||
		menuAnalogDirection(remoteState.moveX, remoteState.moveY) != 0;

	// A deliberate pad action wins the frame over IR motion. This prevents small
	// sensor-bar jitter from discarding a D-pad press before it reaches the menu.
	if (gameCubePadActivity || remotePadActivity)
	{
		if (g_menuInputOwner == WiiMenuInputOwner::GameCubePad && gameCubePadActivity)
			nextOwner = WiiMenuInputOwner::GameCubePad;
		else if (g_menuInputOwner == WiiMenuInputOwner::RemotePad && remotePadActivity)
			nextOwner = WiiMenuInputOwner::RemotePad;
		else if (gameCubePadActivity)
			nextOwner = WiiMenuInputOwner::GameCubePad;
		else
			nextOwner = WiiMenuInputOwner::RemotePad;
	}
	else if (g_menuInputOwner == WiiMenuInputOwner::GameCubePad &&
		menuPadNavigationHeld(gameCubeState))
	{
		nextOwner = WiiMenuInputOwner::GameCubePad;
	}
	else if (g_menuInputOwner == WiiMenuInputOwner::RemotePad &&
		menuPadNavigationHeld(remoteState))
	{
		nextOwner = WiiMenuInputOwner::RemotePad;
	}
	else if (remoteState.menuPointerActivity)
	{
		nextOwner = WiiMenuInputOwner::RemotePointer;
	}
	else if (g_menuInputOwner == WiiMenuInputOwner::GameCubePad && (gameCubeConnected & 1u) == 0)
	{
		nextOwner = WiiMenuInputOwner::RemotePointer;
	}

	if (nextOwner != g_menuInputOwner)
	{
		g_menuInputOwner = nextOwner;
		g_textInputLastHeld = 0;
		g_latchedTextPressed = 0;
		resetMenuNavigation();
		if (g_menuInputOwner != WiiMenuInputOwner::RemotePointer)
			WiiPointer::resetMenuActivityBaseline();
	}
}

void preparePadMenuState(WiiPadInternal::FrameState& state)
{
	const u32 menuKeys =
		(1u << WiiPadInternal::VK_ESCAPE) |
		(1u << WiiPadInternal::VK_DEBUG);
	state.keys &= menuKeys;
	state.mouse = 0;
	state.wheel = 0;
	state.textInputSnapshot.irValid = false;
	state.moveX = 0.0f;
	state.moveY = 0.0f;
	state.menuScrollY = 0.0f;
}

// Pad cursor mode's version of preparePadMenuState(): the left stick becomes
// cursor motion instead of a direction press, and the mouse buttons and wheel
// survive because the cursor is how the pad drives the screen. WiiPointer
// scales the stick as pixels per second once the mouse is not grabbed.
void preparePadCursorMenuState(WiiPadInternal::FrameState& state)
{
	const u32 menuKeys =
		(1u << WiiPadInternal::VK_ESCAPE) |
		(1u << WiiPadInternal::VK_DEBUG);
	state.keys &= menuKeys;
	state.textInputSnapshot.irValid = false;
	WiiPointer::addStickLook(state.moveX, -state.moveY);   // stick Y up-positive, screen Y down-positive
	state.moveX = 0.0f;
	state.moveY = 0.0f;
	state.menuScrollY = 0.0f;
}

bool padCursorMenuWanted()
{
	return g_inMenu && !g_specializedMenuNavigation &&
		!platformTextInputExclusive() && !platformContainerNavigationActive() &&
		!platformPadRebindExclusive();
}

// The pointer's equivalent of preparePadMenuState(). A screen is open, so the
// gameplay key mapping must not reach it -- the inventory key in particular,
// which GuiContainer::keyTyped() reads as "close me". The pad branches already
// filtered that out; the pointer branch passed the raw mapping straight
// through, so opening the inventory and then moving the Wiimote handed the
// ownership to the pointer and delivered the inventory button again, closing it
// on the spot. Holding the remote still kept ownership on the pad and hid the
// bug entirely.
//
// Unlike the pad version this keeps mouse buttons, the wheel and the IR: those
// are how the pointer actually drives a menu.
void preparePointerMenuState(WiiPadInternal::FrameState& state)
{
	const u32 menuKeys =
		(1u << WiiPadInternal::VK_ESCAPE) |
		(1u << WiiPadInternal::VK_DEBUG);
	state.keys &= menuKeys;
	state.moveX = 0.0f;
	state.moveY = 0.0f;
}

void flushMovement(WiiPadInternal::FrameState& state)
{
	float x = state.moveX;
	float y = state.moveY;
	if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
	if (y > 1.0f) y = 1.0f; else if (y < -1.0f) y = -1.0f;

	state.setKey(WiiPadInternal::VK_FORWARD, y > 0.0f);
	state.setKey(WiiPadInternal::VK_BACK, y < 0.0f);
	state.setKey(WiiPadInternal::VK_LEFT, x < 0.0f);
	state.setKey(WiiPadInternal::VK_RIGHT, x > 0.0f);
}

void flushKeysAndButtons(const WiiPadInternal::FrameState& state)
{
	const u32 changed = state.keys ^ g_previousKeys;
	if (changed)
	{
		for (int i = 0; i < WiiPadInternal::VK_COUNT; ++i)
		{
			const u32 bit = 1u << i;
			if (changed & bit)
				lwjgl::Keyboard::detail::pushKey(kVirtualKeyCodes[i], (state.keys & bit) != 0);
		}
	}
	g_previousKeys = state.keys;

	if ((state.mouse ^ g_previousMouse) & WiiPadInternal::VM_ATTACK)
		lwjgl::Mouse::detail::pushButton(0, (state.mouse & WiiPadInternal::VM_ATTACK) != 0,
		                                 WiiPointer::cursorX(), WiiPointer::cursorY());
	if ((state.mouse ^ g_previousMouse) & WiiPadInternal::VM_USE)
		lwjgl::Mouse::detail::pushButton(1, (state.mouse & WiiPadInternal::VM_USE) != 0,
		                                 WiiPointer::cursorX(), WiiPointer::cursorY());
	g_previousMouse = state.mouse;

	if (state.wheel != 0)
		lwjgl::Mouse::detail::pushWheel(state.wheel, WiiPointer::cursorX(), WiiPointer::cursorY());
}
} // namespace

void wiiSetAlternativeControls(bool enabled)
{
	g_alternativeControls = enabled;
}

void wiiSetStickDeadzone(float deadzone)
{
	if (deadzone < 0.05f)
		deadzone = 0.05f;
	if (deadzone > 0.35f)
		deadzone = 0.35f;
	g_stickDeadzone = deadzone;
}

WiiTextInputSnapshot wiiTextInputSnapshot()
{
	return g_textInputSnapshot;
}

std::uint32_t wiiTextInputConsumePressed()
{
	const u32 pressed = g_latchedTextPressed;
	g_latchedTextPressed = 0;
	return pressed;
}

WiiStickSnapshot wiiStickSnapshot()
{
	return g_stickSnapshot;
}

bool wiiMenuPointerActive()
{
	return g_menuInputOwner == WiiMenuInputOwner::RemotePointer || g_padCursorActive;
}

void wiiSetCursorPosition(int x, int y)
{
	WiiPointer::setCursorPosition(x, y);
}

void wiiPadPoll(std::uint32_t gameCubeConnected, bool inMenu, bool specializedMenuNavigation)
{
	g_inMenu = inMenu;
	g_specializedMenuNavigation = specializedMenuNavigation;

	syncVirtualKeyCodes();
	WiiPointer::beginFrame();
	wiiInputDebugReset();

	WiiPadInternal::FrameState gameCubeState;
	WiiPadInternal::FrameState remoteState;
	configureFrameState(gameCubeState);
	configureFrameState(remoteState);

	WiiGameCubePad::poll(gameCubeConnected, gameCubeState);
	WiiRemote::poll(remoteState);
	updateMenuInputOwner(gameCubeConnected, gameCubeState, remoteState);

	WiiPadInternal::FrameState state;
	configureFrameState(state);
	u32 repeatedMenuPressed = 0;
	const bool synthesizeMenuRepeat = !platformTextInputExclusive() &&
		!platformContainerNavigationActive() && !platformPadRebindExclusive();
	// Gameplay when no screen is open. This used to test lwjgl::Mouse::isGrabbed(),
	// which is a proxy for the same thing only as long as something keeps the grab
	// state honest -- and on the Wii nothing did, so a screen could be open with the
	// grab still set and every button kept its in-world mapping.
	const bool regularPadCursor = padCursorMenuWanted() &&
		(g_menuInputOwner == WiiMenuInputOwner::GameCubePad ||
		 (g_menuInputOwner == WiiMenuInputOwner::RemotePad && remoteState.classicAttached));
	const bool containerPadCursorCapable = inMenu && platformContainerNavigationActive() &&
		!platformTextInputExclusive() && !platformPadRebindExclusive() &&
		(g_menuInputOwner == WiiMenuInputOwner::GameCubePad ||
		 (g_menuInputOwner == WiiMenuInputOwner::RemotePad && remoteState.classicAttached));
	const WiiPadInternal::FrameState *containerPadState = nullptr;
	if (containerPadCursorCapable)
		containerPadState = g_menuInputOwner == WiiMenuInputOwner::GameCubePad ? &gameCubeState : &remoteState;
	const bool containerStickActive = containerPadState != nullptr &&
		(containerPadState->moveX != 0.0f || containerPadState->moveY != 0.0f);
	const bool containerDpadHeld = containerPadState != nullptr &&
		(containerPadState->textInputHeld & WII_MENU_DIRECTION_MASK) != 0;
	const WiiContainerCursorPolicy::Decision containerCursor = WiiContainerCursorPolicy::update(
		g_padCursorActive, containerPadCursorCapable, containerStickActive, containerDpadHeld);
	const bool padCursor = containerPadCursorCapable ? containerCursor.pointerActive : regularPadCursor;
	if (padCursor && !g_padCursorActive && !containerPadCursorCapable)
		WiiPointer::centerCursor();
	g_padCursorActive = padCursor;

	if (!inMenu)
	{
		resetMenuNavigation();
		(void)updateGameCubeMenuScroll(0.0f, false);
		mergeFrameState(state, gameCubeState);
		mergeFrameState(state, remoteState);
	}
	else if (padCursor)
	{
		resetMenuNavigation();
		state = g_menuInputOwner == WiiMenuInputOwner::GameCubePad ? gameCubeState : remoteState;
		const int menuWheel = updateGameCubeMenuScroll(state.menuScrollY, true);
		preparePadCursorMenuState(state);
		state.wheel += menuWheel;
	}
	else if (g_menuInputOwner == WiiMenuInputOwner::GameCubePad)
	{
		state = gameCubeState;
		const int menuWheel = updateGameCubeMenuScroll(state.menuScrollY, true);
		if (!containerCursor.analogNavigation)
		{
			resetMenuNavigation();
			if (padCursor)
				preparePadCursorMenuState(state);
			else
				preparePadMenuState(state);
		}
		else
		{
			repeatedMenuPressed = updateMenuNavigation(state, true, synthesizeMenuRepeat);
			preparePadMenuState(state);
		}
		state.wheel += menuWheel;
	}
	else if (g_menuInputOwner == WiiMenuInputOwner::RemotePad)
	{
		state = remoteState;
		const int menuWheel = updateGameCubeMenuScroll(
			state.menuScrollY, !containerCursor.analogNavigation && remoteState.classicAttached);
		if (!containerCursor.analogNavigation)
		{
			resetMenuNavigation();
			if (padCursor)
				preparePadCursorMenuState(state);
			else
				preparePadMenuState(state);
		}
		else
		{
			repeatedMenuPressed = updateMenuNavigation(state, true, synthesizeMenuRepeat);
			if (platformContainerNavigationActive() && !remoteState.classicAttached &&
				!remoteState.textInputSnapshot.irValid)
			{
				WiiPointer::addStickLook(state.moveX, -state.moveY);
			}
			preparePadMenuState(state);
		}
		state.wheel += menuWheel;
	}
	else
	{
		resetMenuNavigation();
		(void)updateGameCubeMenuScroll(0.0f, false);
		state = remoteState;
		preparePointerMenuState(state);
	}

	g_textInputSnapshot = state.textInputSnapshot;
	g_stickSnapshot = state.stickSnapshot;
	updateTextInputSnapshot(state.textInputHeld, repeatedMenuPressed);

	if (platformTextInputExclusive())
	{
		state.keys = 0;
		state.mouse = 0;
		state.wheel = 0;
	}

	if (inMenu != g_previousInMenu)
	{
		g_suppressedKeys = state.keys;
		g_suppressedMouse = state.mouse;
		g_latchedTextPressed = 0;
		resetMenuNavigation();
		// Leaving a menu, the accumulated pointer motion belongs to the cursor,
		// not to the camera. Without this the view snaps on the first frame in
		// world.
		lwjgl::Mouse::clearDeltas();
	}
	g_previousInMenu = inMenu;

	// Release the hold as soon as the player lets go, then apply it.
	g_suppressedKeys &= state.keys;
	g_suppressedMouse &= state.mouse;
	state.keys &= ~g_suppressedKeys;
	state.mouse &= ~g_suppressedMouse;

	flushMovement(state);
	WiiPointer::flush();
	flushKeysAndButtons(state);
}

#endif // WII_PLATFORM
