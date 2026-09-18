#ifdef PS2_PLATFORM

#include "ps2/boot/Ps2BootScreen.h"
#include "ps2/boot/Ps2BootRenderer.h"
#include "ps2/boot/Ps2BootText.h"
#include <delaythread.h>

namespace
{

constexpr int kTextZ = 0xFFFF;
constexpr Ps2BootRenderer::Color kBlack = {0x00, 0x00, 0x00, 0x80};
constexpr Ps2BootRenderer::Color kWhite = {0xE8, 0xE8, 0xE8, 0x80};
constexpr Ps2BootRenderer::Color kMuted = {0xA8, 0xA8, 0xA8, 0x80};

void drawMissingAssetsScreen()
{
    const float centerX = static_cast<float>(Ps2BootRenderer::width()) * 0.5f;
    const float centerY = static_cast<float>(Ps2BootRenderer::height()) * 0.5f;

    Ps2BootRenderer::setAlphaBlend(false);
    Ps2BootRenderer::clear(kBlack);

    Ps2BootText::drawCentered(centerX, centerY - 84.0f, kTextZ,
                              "Assets couldn't be loaded.", 3.0f, kWhite);
    Ps2BootText::drawCentered(centerX, centerY - 38.0f, kTextZ,
                              "Make sure the game files are located in:", 2.0f, kMuted);
    Ps2BootText::drawCentered(centerX, centerY + 2.0f, kTextZ,
                              "Wii SD: sd:/apps/OptiCraft", 2.0f, kWhite);
    Ps2BootText::drawCentered(centerX, centerY + 28.0f, kTextZ,
                              "Wii USB: usb:/apps/OptiCraft", 2.0f, kWhite);
    Ps2BootText::drawCentered(centerX, centerY + 54.0f, kTextZ,
                              "PS2 USB: mass:/OptiCraftHeritage", 2.0f, kWhite);

    Ps2BootRenderer::present();
}

} // namespace

[[noreturn]] void ps2HaltNoData()
{
    for (;;)
        drawMissingAssetsScreen();
}

[[noreturn]] void ps2HaltBlack()
{
    for (;;) {
        Ps2BootRenderer::clear({0, 0, 0, 0x80});
        Ps2BootRenderer::present();
        DelayThread(1000000);
    }
}

#endif // PS2_PLATFORM
