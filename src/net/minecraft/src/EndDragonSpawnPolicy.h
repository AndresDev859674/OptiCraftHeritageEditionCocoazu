#pragma once

#include "java/Type.h"

// Decides whether End decoration may create the Ender Dragon. Kept free of
// world state so the gate is testable: the decorator resolves the inputs.
namespace EndDragonSpawnPolicy
{
	// Decoration coordinates are block-based (chunkX * 16), so the central
	// chunk is the one whose block origin is (0, 0).
	inline bool isCentralChunk(int_t blockX, int_t blockZ)
	{
		return blockX == 0 && blockZ == 0;
	}

	inline bool shouldSpawnDragon(int_t blockX, int_t blockZ, bool defeated, bool liveDragonPresent)
	{
		return isCentralChunk(blockX, blockZ) && !defeated && !liveDragonPresent;
	}
}
