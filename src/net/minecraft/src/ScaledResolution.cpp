#include "ScaledResolution.h"

#include <cmath>
#include "java/Arithmetic.h"
#include "GameSettings.h"
#include "platform/ConsoleAspectRatio.h"
#include "platform/PlatformTuning.h"

ScaledResolution::ScaledResolution(GameSettings *gamesettings, int_t i, int_t j)
{
	const bool widescreen = gamesettings != nullptr && gamesettings->widescreen;
	scaledWidth = ConsoleAspectRatio::getLogicalWidth(i, j, widescreen);
	scaledHeight = ConsoleAspectRatio::getLogicalHeight(j);
	scaleFactor = 1;
	exactScaleFactor = 1.0;
	if (gamesettings != nullptr && gamesettings->legacyUI && PLATFORM_LEGACY_GUI_SCALE > 0.0)
	{
		exactScaleFactor = PLATFORM_LEGACY_GUI_SCALE;
	}
	else
	{
#if PLATFORM_CONSOLE_LOW && PLATFORM_FORCE_GUI_SCALE > 0
		scaleFactor = PLATFORM_FORCE_GUI_SCALE;
		while (scaleFactor > 1 && (scaledWidth / scaleFactor < 1 || scaledHeight / scaleFactor < 1))
			scaleFactor--;
		exactScaleFactor = static_cast<double>(scaleFactor);
#else
		int_t k = gamesettings->guiScale;
		if (k == 0)
			k = 1000;
		for (; scaleFactor < k && scaledWidth / (scaleFactor + 1) >= 320 && scaledHeight / (scaleFactor + 1) >= 240; scaleFactor++)
		{
		}
		exactScaleFactor = static_cast<double>(scaleFactor);
#endif
	}
	field_25121_a = (double)scaledWidth / exactScaleFactor;
	field_25120_b = (double)scaledHeight / exactScaleFactor;
	scaledWidth = JavaArithmetic::doubleToInt(std::ceil(field_25121_a));
	scaledHeight = JavaArithmetic::doubleToInt(std::ceil(field_25120_b));
}

int_t ScaledResolution::getScaledWidth()
{
	return scaledWidth;
}

int_t ScaledResolution::getScaledHeight()
{
	return scaledHeight;
}


double ScaledResolution::getScaleFactorExact() const
{
	return exactScaleFactor;
}
