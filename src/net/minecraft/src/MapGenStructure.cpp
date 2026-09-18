#include "MapGenStructure.h"

#include <cmath>
#include <limits>

#include "ChunkCoordIntPair.h"
#include "ChunkPosition.h"
#include "StructureBoundingBox.h"
#include "StructureComponent.h"
#include "StructureStart.h"
#include "World.h"
#include "platform/world/StructureReservation.h"
#include "java/Arithmetic.h"

namespace
{
	double getJavaStructureDistance(const ChunkPosition &position, int_t x, int_t y, int_t z)
	{
		const int_t dx = JavaArithmetic::intSub(position.x, x);
		const int_t dy = JavaArithmetic::intSub(position.y, y);
		const int_t dz = JavaArithmetic::intSub(position.z, z);
		const int_t mixed = JavaArithmetic::intMul(JavaArithmetic::intMul(dx, dy), dy);
		const int_t horizontal = JavaArithmetic::intMul(dz, dz);
		return static_cast<double>(JavaArithmetic::intAdd(JavaArithmetic::intAdd(dx, mixed), horizontal));
	}

	struct IterationScope
	{
		explicit IterationScope(int_t &depth) : depth_(depth) { ++depth_; }
		~IterationScope() { --depth_; }
		IterationScope(const IterationScope &) = delete;
		IterationScope &operator=(const IterationScope &) = delete;
	private:
		int_t &depth_;
	};
}

MapGenStructure::MapGenStructure()
{
	// Structures use the vanilla 8-chunk source sweep even when cave generation
	// uses a reduced platform radius.
	sourceRange = 8;
}

MapGenStructure::~MapGenStructure()
{
	for (auto &entry : coordMap)
		delete entry.second;
}

std::size_t MapGenStructure::negativeSourceCacheSet(ulong_t key) const
{
	const std::uint32_t chunkX = static_cast<std::uint32_t>(key);
	const std::uint32_t chunkZ = static_cast<std::uint32_t>(key >> 32);
	std::uint32_t hash = chunkX * 73428767u ^ chunkZ * 912931u;
	hash ^= hash >> 16;
	return static_cast<std::size_t>(hash) & (NEGATIVE_SOURCE_CACHE_SET_COUNT - 1);
}

bool MapGenStructure::isNegativeSourceCached(ulong_t key) const
{
	const std::size_t set = negativeSourceCacheSet(key);
	const std::size_t firstSlot = set * NEGATIVE_SOURCE_CACHE_WAYS;
	for (std::size_t way = 0; way < NEGATIVE_SOURCE_CACHE_WAYS; ++way)
	{
		const std::size_t slot = firstSlot + way;
		const std::uint32_t mask = 1u << (slot & 31);
		if ((negativeSourceCacheValid[slot >> 5] & mask) != 0 &&
			negativeSourceCache[slot] == key)
		{
			return true;
		}
	}
	return false;
}

void MapGenStructure::cacheNegativeSource(ulong_t key)
{
	const std::size_t set = negativeSourceCacheSet(key);
	const std::size_t firstSlot = set * NEGATIVE_SOURCE_CACHE_WAYS;
	std::size_t slot = firstSlot;

	for (std::size_t way = 0; way < NEGATIVE_SOURCE_CACHE_WAYS; ++way)
	{
		const std::size_t candidate = firstSlot + way;
		const std::uint32_t mask = 1u << (candidate & 31);
		if ((negativeSourceCacheValid[candidate >> 5] & mask) == 0)
		{
			slot = candidate;
			negativeSourceCache[slot] = key;
			negativeSourceCacheValid[slot >> 5] |= mask;
			return;
		}
	}

	const std::size_t way = negativeSourceCacheNextWay[set] & (NEGATIVE_SOURCE_CACHE_WAYS - 1);
	negativeSourceCacheNextWay[set] = static_cast<std::uint8_t>((way + 1) & (NEGATIVE_SOURCE_CACHE_WAYS - 1));
	slot = firstSlot + way;
	negativeSourceCache[slot] = key;
}

bool MapGenStructure::sourceNeedsGeneration(int_t sourceChunkX, int_t sourceChunkZ)
{
	const ulong_t key = ChunkCoordIntPair::chunkXZ2Long(sourceChunkX, sourceChunkZ);
	return coordMap.find(key) == coordMap.end() && !isNegativeSourceCached(key);
}

void MapGenStructure::generateChunk(World *world, int_t sourceChunkX, int_t sourceChunkZ,
                                    int_t, int_t, byte_t *)
{
	const ulong_t key = ChunkCoordIntPair::chunkXZ2Long(sourceChunkX, sourceChunkZ);
	if (coordMap.find(key) != coordMap.end() || isNegativeSourceCached(key))
		return;

	rand.nextInt();
	if (canSpawnStructureAtCoords(sourceChunkX, sourceChunkZ))
	{
		coordMap.emplace(key, getStructureStart(sourceChunkX, sourceChunkZ));
		coordOrder.add(key);
	}
	else
	{
		cacheNegativeSource(key);
	}
}

bool MapGenStructure::generateStructuresInChunk(World *world, Random &random,
                                                int_t chunkX, int_t chunkZ)
{
	const int_t minX = JavaArithmetic::intAdd(JavaArithmetic::intShl(chunkX, 4), 8);
	const int_t minZ = JavaArithmetic::intAdd(JavaArithmetic::intShl(chunkZ, 4), 8);
	const int_t maxX = JavaArithmetic::intAdd(minX, 15);
	const int_t maxZ = JavaArithmetic::intAdd(minZ, 15);
	const StructureBoundingBox chunkBounds(minX, minZ, maxX, maxZ);
	bool generated = false;

	// generateStructure() places blocks and measures ground level, which loads
	// neighbouring chunks; a chunk that is not resident yet is generated on the spot,
	// and ChunkProviderGenerate::provideChunk runs this generator's own source sweep,
	// re-entering generateChunk() and inserting into coordMap. A rehash then
	// invalidates the iterator held here. Java only risked a
	// ConcurrentModificationException; in C++ it is a use-after-free, so iterate over a
	// snapshot. Entries are only ever added to coordMap, never erased before teardown,
	// so a snapshotted pointer stays valid for the whole loop.
	const std::vector<ulong_t> structureKeys = coordOrder.valuesInIterationOrder();
	const IterationScope iterationScope(structureIterationDepth);
	for (ulong_t key : structureKeys)
	{
		auto it = coordMap.find(key);
		StructureStart *start = it != coordMap.end() ? it->second : nullptr;
		if (start != nullptr && start->isSizeableStructure() && start->getBoundingBox() != nullptr &&
		    start->getBoundingBox()->intersectsWith(minX, minZ, maxX, maxZ))
		{
			start->generateStructure(world, random, chunkBounds);
			generated = true;
		}
	}
	return generated;
}

void MapGenStructure::trimStructureStarts(int_t minBlockX, int_t minBlockZ, int_t maxBlockX, int_t maxBlockZ)
{
	if (structureIterationDepth > 0)
		return;

	for (auto it = coordMap.begin(); it != coordMap.end(); )
	{
		StructureStart *start = it->second;
		const bool retained = start != nullptr && start->getBoundingBox() != nullptr &&
		    start->getBoundingBox()->intersectsWith(minBlockX, minBlockZ, maxBlockX, maxBlockZ);
		if (retained)
		{
			++it;
			continue;
		}

		coordOrder.remove(it->first);
		delete start;
		it = coordMap.erase(it);
	}
}

void MapGenStructure::appendIntersectingComponentBounds(
	const StructureBoundingBox &area, std::vector<StructureBoundingBox> &out,
	int_t horizontalPadding) const
{
	const bool projectToAreaHeight = projectDecorationReservationToAreaHeight();
	const int_t queryMinX = JavaArithmetic::intSub(area.minX, horizontalPadding);
	const int_t queryMinZ = JavaArithmetic::intSub(area.minZ, horizontalPadding);
	const int_t queryMaxX = JavaArithmetic::intAdd(area.maxX, horizontalPadding);
	const int_t queryMaxZ = JavaArithmetic::intAdd(area.maxZ, horizontalPadding);

	for (const auto &entry : coordMap)
	{
		StructureStart *start = entry.second;
		if (start == nullptr || !start->isSizeableStructure() || start->getBoundingBox() == nullptr ||
		    !start->getBoundingBox()->intersectsWith(queryMinX, queryMinZ, queryMaxX, queryMaxZ))
		{
			continue;
		}

		for (StructureComponent *component : start->getComponents())
		{
			StructureBoundingBox *componentBounds =
				component != nullptr ? component->getBoundingBox() : nullptr;
			if (componentBounds == nullptr)
				continue;

			const StructureBoundingBox reservedBounds = StructureReservation::makeBounds(
				*componentBounds, area, projectToAreaHeight, horizontalPadding);
			if (reservedBounds.intersectsWith(area))
				out.push_back(reservedBounds);
		}
	}
}

bool MapGenStructure::isInsideStructure(int_t x, int_t y, int_t z) const
{
	const std::vector<ulong_t> structureKeys = coordOrder.valuesInIterationOrder();
	for (ulong_t key : structureKeys)
	{
		auto it = coordMap.find(key);
		StructureStart *start = it != coordMap.end() ? it->second : nullptr;
		if (start == nullptr || !start->isSizeableStructure() || start->getBoundingBox() == nullptr ||
		    !start->getBoundingBox()->intersectsWith(x, z, x, z))
			continue;

		for (StructureComponent *component : start->getComponents())
		{
			if (component != nullptr && component->getBoundingBox() != nullptr &&
			    component->getBoundingBox()->isVecInside(x, y, z))
				return true;
		}
	}
	return false;
}

ChunkPosition *MapGenStructure::getNearestInstance(World *world, int_t x, int_t y, int_t z)
{
	worldObj = world;
	rand.setSeed(world->getRandomSeed());
	const long_t xMultiplier = rand.nextLong();
	const long_t zMultiplier = rand.nextLong();
	const long_t xSeed = JavaArithmetic::longMul(static_cast<long_t>(JavaArithmetic::intShr(x, 4)), xMultiplier);
	const long_t zSeed = JavaArithmetic::longMul(static_cast<long_t>(JavaArithmetic::intShr(z, 4)), zMultiplier);
	const ulong_t seedBits = static_cast<ulong_t>(xSeed) ^ static_cast<ulong_t>(zSeed) ^
	                         static_cast<ulong_t>(world->getRandomSeed());
	rand.setSeed(JavaArithmetic::longFromBits(seedBits));
	generateChunk(world, JavaArithmetic::intShr(x, 4), JavaArithmetic::intShr(z, 4), 0, 0, nullptr);

	double bestDistance = std::numeric_limits<double>::max();
	ChunkPosition *best = nullptr;
	const std::vector<ulong_t> structureKeys = coordOrder.valuesInIterationOrder();
	for (ulong_t key : structureKeys)
	{
		auto it = coordMap.find(key);
		StructureStart *start = it != coordMap.end() ? it->second : nullptr;
		if (start == nullptr || !start->isSizeableStructure() || start->getComponents().empty())
			continue;

		ChunkPosition *position = start->getComponents().front()->getCenter();
		if (position == nullptr)
			continue;
		const double distance = getJavaStructureDistance(*position, x, y, z);
		if (distance < bestDistance)
		{
			delete best;
			bestDistance = distance;
			best = position;
		}
		else
		{
			delete position;
		}
	}

	if (best != nullptr)
		return best;

	std::vector<ChunkPosition *> coordinates = getStructureCoordinates();
	for (ChunkPosition *position : coordinates)
	{
		if (position == nullptr)
			continue;
		const double distance = getJavaStructureDistance(*position, x, y, z);
		if (distance < bestDistance)
		{
			delete best;
			bestDistance = distance;
			best = new ChunkPosition(*position);
		}
		delete position;
	}
	return best;
}

std::vector<ChunkPosition *> MapGenStructure::getStructureCoordinates()
{
	return {};
}
