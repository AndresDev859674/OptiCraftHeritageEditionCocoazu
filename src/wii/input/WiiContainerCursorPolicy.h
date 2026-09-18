#pragma once

namespace WiiContainerCursorPolicy
{

struct Decision
{
    bool pointerActive = false;
    bool analogNavigation = true;
};

inline Decision update(bool previousPointerActive, bool pointerCapable,
                       bool leftStickActive, bool dpadHeld)
{
    Decision decision;
    if (!pointerCapable)
        return decision;

    decision.analogNavigation = false;
    decision.pointerActive = previousPointerActive;

    if (dpadHeld)
        decision.pointerActive = false;
    else if (leftStickActive)
        decision.pointerActive = true;

    return decision;
}

} // namespace WiiContainerCursorPolicy
