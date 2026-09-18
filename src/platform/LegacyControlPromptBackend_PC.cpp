#include "platform/LegacyControlPromptBackend.h"

#include "net/minecraft/src/GameSettings.h"
#include "net/minecraft/src/KeyBinding.h"

namespace
{
const KeyBinding *bindingForAction(const GameSettings &settings, LegacyControlAction action)
{
    switch (action)
    {
    case LegacyControlAction::Inventory: return settings.keyBindInventory;
    case LegacyControlAction::Drop: return settings.keyBindDrop;
    case LegacyControlAction::Jump: return settings.keyBindJump;
    case LegacyControlAction::Attack: return settings.keyBindAttack;
    case LegacyControlAction::Use: return settings.keyBindUseItem;
    }
    return nullptr;
}
}

std::string legacyControlPromptLabel(const GameSettings &settings, LegacyControlAction action)
{
    const KeyBinding *binding = bindingForAction(settings, action);
    if (binding == nullptr)
        return std::string();
    return GameSettings::getKeyDisplayString(binding->keyCode);
}
