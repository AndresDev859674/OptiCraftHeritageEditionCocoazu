#include "platform/PlatformUserSettings.h"

#include "wii/gx_wii.h"
#include "wii/input/WiiPadState.h"

namespace PlatformUserSettings
{
void setControllerDeadzone(float value)
{
	wiiSetStickDeadzone(value);
}

void setAlternativeControls(bool enabled)
{
	wiiSetAlternativeControls(enabled);
}

void setDisplayDeflicker(bool enabled)
{
	wiigl_set_deflicker_enabled(enabled);
}
}
