#pragma once

#include <string>

#include "MapDataBase.h"

class World;

// Persistent state of the one Ender Dragon that belongs to a save. Stored as
// MapDataBase under a fixed key so it lives in the world's data/ directory next
// to the map items; dimension worlds share the save handler, so the key
// identifies the End of this save. An absent or unreadable file reads as "not
// defeated", which keeps old worlds compatible.
class EndDragonState : public MapDataBase
{
public:
	static const std::string STORAGE_KEY;

	explicit EndDragonState(const std::string &s);

	void readFromNBT(NBTTagCompound *nbttagcompound) override;
	void writeToNBT(NBTTagCompound *nbttagcompound) override;

	bool isDefeated() const;
	void setDefeated();

	// Loads the saved state or registers a fresh, undefeated one. Never null.
	static EndDragonState *get(World *world);
	// MapStorage factory (see MapStorage::loadData).
	static MapDataBase *create(const std::string &s);

private:
	bool defeated;
};
