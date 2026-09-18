#include "EndDragonState.h"

#include "NBTTagCompound.h"
#include "World.h"

const std::string EndDragonState::STORAGE_KEY = "EndDragonState";

EndDragonState::EndDragonState(const std::string &s) :
	MapDataBase(s),
	defeated(false)
{
}

void EndDragonState::readFromNBT(NBTTagCompound *nbttagcompound)
{
	defeated = nbttagcompound != nullptr && nbttagcompound->getBoolean("defeated");
}

void EndDragonState::writeToNBT(NBTTagCompound *nbttagcompound)
{
	nbttagcompound->setBoolean("defeated", defeated);
}

bool EndDragonState::isDefeated() const
{
	return defeated;
}

void EndDragonState::setDefeated()
{
	defeated = true;
	markDirty();
}

EndDragonState *EndDragonState::get(World *world)
{
	EndDragonState *state = dynamic_cast<EndDragonState *>(world->loadItemData(STORAGE_KEY, &EndDragonState::create));
	if (state == nullptr)
	{
		state = new EndDragonState(STORAGE_KEY);
		world->setItemData(STORAGE_KEY, state);
	}
	return state;
}

MapDataBase *EndDragonState::create(const std::string &s)
{
	return new EndDragonState(s);
}
