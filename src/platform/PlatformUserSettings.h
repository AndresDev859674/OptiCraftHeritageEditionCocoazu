#pragma once

namespace PlatformUserSettings
{
void setControllerDeadzone(float value);
void setAlternativeControls(bool enabled);
// Wii: vertical deflicker filter on the display copy. No-op elsewhere.
void setDisplayDeflicker(bool enabled);
}
