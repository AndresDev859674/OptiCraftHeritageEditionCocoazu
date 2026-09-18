#include "LegacySceneLayout.h"

#include <algorithm>


int_t legacyScaleToScreen(int_t screenHeight, int_t referenceValue, int_t minimum, int_t maximum)
{
    const int_t safeHeight = std::max<int_t>(1, screenHeight);
    const int_t scaled = referenceValue * safeHeight / LEGACY_REFERENCE_HEIGHT;
    return std::max<int_t>(minimum, std::min<int_t>(maximum, scaled));
}

LegacySceneLayout legacySceneLayout(int_t screenWidth, int_t screenHeight)
{
    LegacySceneLayout scene{};
    const int_t safeWidth = std::max<int_t>(1, screenWidth);
    const int_t safeHeight = std::max<int_t>(1, screenHeight);

    // Legacy drops the logo about a tenth of the way down the screen and gives it
    // a little under a fifth of the height, so it reads as centred in the upper
    // band rather than stuck against the top edge.
    scene.titleY = std::max<int_t>(4, safeHeight * 10 / 100);
    scene.titleMaxHeight = std::max<int_t>(24, safeHeight * 19 / 100);

    // The width budget is what actually sizes the logo on a 4:3 screen. Holding it
    // at a share of the width keeps the logo the same relative size on a console
    // as on the desktop; on a wider screen the height budget takes over, which is
    // what keeps the logo from growing with the aspect ratio.
    scene.titleMaxWidth = std::min<int_t>(std::max<int_t>(96, safeWidth - 24), safeWidth * 68 / 100);

    // The logo is a wide banner, so on a 4:3 screen the width budget is what
    // decides its height and the height budget goes unused. Reserving the full
    // budget below it would leave a hole and push every panel anchored at
    // contentTop down for nothing, so reserve what the banner actually occupies.
    // Screens fall back to the shorter Java logo when the asset is missing, which
    // never exceeds this either.
    const int_t bannerHeight = std::min<int_t>(scene.titleMaxHeight,
        scene.titleMaxWidth * LEGACY_TITLE_ASPECT_HEIGHT / LEGACY_TITLE_ASPECT_WIDTH);
    scene.contentTop = scene.titleY + bannerHeight + std::max<int_t>(4, safeHeight / 40);
    return scene;
}
