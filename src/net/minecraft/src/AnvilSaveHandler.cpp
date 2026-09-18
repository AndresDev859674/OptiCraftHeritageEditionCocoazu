#include "AnvilSaveHandler.h"

#include "AnvilChunkLoader.h"
#include "WorldInfo.h"
#include "WorldProvider.h"
#include "WorldProviderHell.h"
#include "WorldProviderEnd.h"
#include "platform/Storage.h"

AnvilSaveHandler::AnvilSaveHandler(const std::string &baseDir, const std::string &name,
                                   bool createPlayers, bool readOnly)
    : SaveHandler(baseDir, name, createPlayers, readOnly)
{
}

IChunkLoader *AnvilSaveHandler::getChunkLoader(WorldProvider *worldprovider)
{
    if (dynamic_cast<WorldProviderHell *>(worldprovider) != nullptr)
    {
        const std::string dimensionDir = PlatformStorage::join(getSaveDirectory(), "DIM-1");
        if (!isReadOnly())
            PlatformStorage::mkdirs(dimensionDir);
        return new AnvilChunkLoader(dimensionDir, isReadOnly());
    }

    if (dynamic_cast<WorldProviderEnd *>(worldprovider) != nullptr)
    {
        const std::string dimensionDir = PlatformStorage::join(getSaveDirectory(), "DIM1");
        if (!isReadOnly())
            PlatformStorage::mkdirs(dimensionDir);
        return new AnvilChunkLoader(dimensionDir, isReadOnly());
    }

    return new AnvilChunkLoader(getSaveDirectory(), isReadOnly());
}

void AnvilSaveHandler::saveWorldInfoAndPlayer(WorldInfo *worldinfo,
                                               const std::vector<EntityPlayer *> &players)
{
    if (worldinfo != nullptr)
        worldinfo->setSaveVersion(19133);
    SaveHandler::saveWorldInfoAndPlayer(worldinfo, players);
}
