#include "LegacySceneState.h"

#include "java/System.h"

int_t legacyScenePanoramaTimerFromMillis(long_t millis)
{
    // Keep the timer in a small integer domain before LegacyPanorama converts it
    // to float.  Using the full wall-clock tick count loses sub-second precision
    // after long uptimes because float cannot represent adjacent large integers.
    const long_t periodTicks = 3600;
    long_t ticks = millis / 50;
    long_t wrapped = ticks % periodTicks;
    if (wrapped < 0)
        wrapped += periodTicks;
    return static_cast<int_t>(wrapped);
}

int_t legacyScenePanoramaTimer()
{
    return legacyScenePanoramaTimerFromMillis(System::currentTimeMillis());
}
