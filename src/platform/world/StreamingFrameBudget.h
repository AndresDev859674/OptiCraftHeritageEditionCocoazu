#pragma once

#include "java/Type.h"

// Shared wall-clock allowance for the world-side streaming drains of one
// rendered frame: the generation slices (tick and frame), the deferred
// populate queue and the lighting queue.
//
// Each drain keeps its own per-call ceiling (PLATFORM_GENERATION_BUDGET_US,
// PLATFORM_POPULATE_BUDGET_US, ...). Those were sized one at a time, and a
// frame that runs a tick pays all of them in full: generation, then populate,
// then lighting, then the frame generation slice, before a single triangle is
// drawn. This bounds their sum instead. A drain asks for its ceiling clamped to
// what the frame has left, and charges what it spent on the way out.
//
// Mesh building is deliberately not a consumer. It runs last in the frame and
// is what turns a streamed chunk into something visible; putting it behind
// generation in a shared pool would trade the hitch for holes.
//
// Every consumer checks its clock after a step, never before, so an exhausted
// allowance degrades to one step per drain per frame rather than a stall.
// PLATFORM_STREAMING_FRAME_BUDGET_US at 0 leaves each drain on its own ceiling.
class PlatformStreamingFrameBudget
{
public:
    // Called once per rendered frame, before the tick loop.
    static void beginFrame();
    // Own ceiling in microseconds (0 = unbounded) clamped to what the frame has
    // left. Before the first beginFrame, and with the feature off, it is
    // returned unchanged.
    static long_t clampUs(long_t ownBudgetUs);
    static void consumeUs(long_t spentUs);

private:
    static bool frameActive;
    static long_t remainingUs;
};

// Charges the wall time of the enclosing drain to the frame allowance on exit.
class PlatformStreamingFrameBudgetScope
{
public:
    PlatformStreamingFrameBudgetScope();
    ~PlatformStreamingFrameBudgetScope();

    PlatformStreamingFrameBudgetScope(const PlatformStreamingFrameBudgetScope &) = delete;
    PlatformStreamingFrameBudgetScope &operator=(const PlatformStreamingFrameBudgetScope &) = delete;

private:
    long_t startUs;
};
