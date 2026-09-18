#include "platform/world/StreamingFrameBudget.h"

#include "platform/PlatformCompat.h"
#include "platform/PlatformTuning.h"

bool PlatformStreamingFrameBudget::frameActive = false;
long_t PlatformStreamingFrameBudget::remainingUs = 0;

void PlatformStreamingFrameBudget::beginFrame()
{
    if (PLATFORM_STREAMING_FRAME_BUDGET_US <= 0)
        return;
    frameActive = true;
    remainingUs = static_cast<long_t>(PLATFORM_STREAMING_FRAME_BUDGET_US);
}

long_t PlatformStreamingFrameBudget::clampUs(long_t ownBudgetUs)
{
    if (PLATFORM_STREAMING_FRAME_BUDGET_US <= 0 || !frameActive)
        return ownBudgetUs;
    // A drain that would be stopped before its first step would still run it,
    // because the consumers test the clock after a step. 1 keeps the value a
    // positive "bounded" budget for callers that treat 0 as unbounded.
    const long_t left = remainingUs > 1 ? remainingUs : 1;
    if (ownBudgetUs <= 0 || ownBudgetUs > left)
        return left;
    return ownBudgetUs;
}

void PlatformStreamingFrameBudget::consumeUs(long_t spentUs)
{
    if (!frameActive || spentUs <= 0)
        return;
    remainingUs = spentUs >= remainingUs ? 0 : remainingUs - spentUs;
}

PlatformStreamingFrameBudgetScope::PlatformStreamingFrameBudgetScope()
    : startUs(static_cast<long_t>(PlatformCompat::getMonotonicMicros()))
{
}

PlatformStreamingFrameBudgetScope::~PlatformStreamingFrameBudgetScope()
{
    const long_t nowUs = static_cast<long_t>(PlatformCompat::getMonotonicMicros());
    if (nowUs > startUs)
        PlatformStreamingFrameBudget::consumeUs(nowUs - startUs);
}
