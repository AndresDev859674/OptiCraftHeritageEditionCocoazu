#pragma once

struct PlatformGameDefaults
{
    bool usePerformanceProfile = false;
    int renderDistance = 0;
    bool fancyGraphics = true;
    bool ambientOcclusion = true;
    int particleSetting = 0;
    int limitFramerate = 1;
    bool viewBobbing = true;
    bool fogOff = false;
    float brightness = 0.0f;
    float aoLevel = 0.0f;
    bool smoothFps = false;
    int autoSaveTicks = 4000;
    bool weather = true;
    bool sky = true;
    bool sunMoon = true;
    int clouds = 1;
    bool stars = true;
    int chunkUpdates = 1;
    bool chunkUpdatesDynamic = true;
    int mipmapLevel = 0;
};

const PlatformGameDefaults& platformGameDefaults();
