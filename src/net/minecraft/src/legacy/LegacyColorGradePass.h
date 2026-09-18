#pragma once

class Minecraft;

// Applies the desktop fixed-function Legacy color grade to the already rendered
// 3D scene. Console builds intentionally keep this as a no-op and use the
// lightmap/fog/sky fallback from LegacyLook instead.
void legacyLookApplyWorldGrade(Minecraft *mc);
