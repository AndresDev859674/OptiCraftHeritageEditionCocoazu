#pragma once

#include <string>

class NBTTagCompound;
class MapDataBase;

// Constructs the concrete MapDataBase that MapStorage should fill from a saved
// file. Storage stays generic; each saved data type supplies its own factory.
using MapDataFactory = MapDataBase *(*)(const std::string &name);

// net.minecraft.src.MapDataBase
class MapDataBase
{
public:
	MapDataBase(const std::string &s);
	virtual ~MapDataBase() = default;

	virtual void readFromNBT(NBTTagCompound *nbttagcompound) = 0;
	virtual void writeToNBT(NBTTagCompound *nbttagcompound) = 0;

	void markDirty();
	void setDirty(bool flag);
	bool isDirty();

	std::string mapName; // mapName

private:
	bool dirty;
};
