#pragma once

class PcLegacyLightmapCache
{
public:
    explicit PcLegacyLightmapCache(int intervalTicks)
        : interval(intervalTicks > 0 ? intervalTicks : 1)
    {
    }

    // legacyLook is part of the key: the Legacy grade is baked into the lightmap
    // entries, so toggling it has to rebuild them. Without it the change only
    // showed up whenever the interval happened to expire.
    bool shouldUpdate(int tick, const void *provider, float gamma, bool lightning, bool legacyLook,
        bool dirty) const
    {
        if (!dirty)
            return false;
        if (!initialized || provider != lastProvider || gamma != lastGamma ||
            lightning != lastLightning || legacyLook != lastLegacyLook)
            return true;
        return tick - lastUpdateTick >= interval;
    }

    void markUpdated(int tick, const void *provider, float gamma, bool lightning, bool legacyLook)
    {
        initialized = true;
        lastUpdateTick = tick;
        lastProvider = provider;
        lastGamma = gamma;
        lastLightning = lightning;
        lastLegacyLook = legacyLook;
    }

    void invalidate()
    {
        initialized = false;
    }

private:
    int interval;
    int lastUpdateTick = 0;
    const void *lastProvider = nullptr;
    float lastGamma = 0.0f;
    bool lastLightning = false;
    bool lastLegacyLook = false;
    bool initialized = false;
};
