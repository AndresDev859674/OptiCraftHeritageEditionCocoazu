#include "platform/Log.h"
#include "ChunkProvider.h"
#include "Config.h"

#include <cstdio>
#include <algorithm>
#include <sstream>

#include "World.h"
#include "WorldProvider.h"
#include "Chunk.h"
#include "EmptyChunk.h"
#include "IChunkLoader.h"
#include "IProgressUpdate.h"
#include "ThreadedFileIOBase.h"
#include "ChunkCoordIntPair.h"
#include "ChunkProviderLoadOrGenerate.h"
#include "ChunkProviderGenerate.h"
#include "McRegionChunkLoader.h"
#include "AnvilChunkLoader.h"
#include "java/Arithmetic.h"
#include "java/String.h"
#include "java/System.h"
#include "platform/PlatformTuning.h"
#include "platform/WorldLoadTrace.h"
#include "platform/chunks/ChunkGenerationScheduler.h"
#include "platform/chunks/ChunkMemoryPolicy.h"
#include "platform/world/StreamingFrameBudget.h"
#include "ISaveHandler.h"

#include "platform/Profiler.h"

namespace
{
	int_t clampInt(int_t value, int_t minValue, int_t maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	// Only the unbounded (desktop) path uses this; PLATFORM_BOUNDED_WORLD takes
	// both radii straight from the tuning table instead of deriving one.
	const int_t CACHE_RADIUS_MARGIN = 4;


	#if PLATFORM_DEFERRED_POPULATE
	const int_t POPULATION_FOOTPRINT_AXIS = 2;
	const int_t POPULATION_SECTION_COUNT = 8;
	#endif
}

ChunkProvider::ChunkProvider(World *world, IChunkLoader *ichunkloader, IChunkProvider *ichunkprovider)
#if PLATFORM_ASYNC_CHUNK_GENERATION
: asyncGenerationScheduler(nullptr)
, droppedChunksSet()
#else
: droppedChunksSet()
#endif
, blankChunk(nullptr)
, chunkProvider(ichunkprovider)
, chunkLoader(ichunkloader)
, chunkMap()
, chunkList()
, lastChunk(nullptr)
, lastChunkX(0)
, lastChunkZ(0)
, worldObj(world)
, curChunkX(0)
, curChunkZ(0)
, chunkLoadRadius(15)
, chunkUnloadRadius(15)
, chunkTopologyVersion(1)
{
	blankChunk = new EmptyChunk(world, std::vector<byte_t>(32768, 0), 0, 0);
	#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
	generationMoveX = 0;
	generationMoveZ = 0;
	generationCenterInitialized = false;
	#endif
	#if PLATFORM_BOUNDED_WORLD
	{
		// A resident region (the End) sits on top of the sliding window.
		std::size_t reserve = PLATFORM_CHUNK_MAP_RESERVE;
		const int_t residentRadius = world != nullptr && world->worldProvider != nullptr
		? world->worldProvider->getResidentChunkRadius() : -1;
		if (residentRadius >= 0)
			reserve += static_cast<std::size_t>(residentRadius * 2 + 1) * (residentRadius * 2 + 1);
		chunkMap.reserve(reserve);
		chunkList.reserve(reserve);
	}
	genChunksThisTick = 0;
	setChunkLoadRadius(PLATFORM_CHUNK_CACHE_RADIUS);
	#else
	chunkMap.reserve(256);
	chunkList.reserve(256);
	#endif
	#if PLATFORM_ASYNC_CHUNK_GENERATION
	asyncGenerationScheduler = nullptr;
	asyncSavedChunkProbe = nullptr;
	McRegionChunkLoader *asyncRegionLoader = dynamic_cast<McRegionChunkLoader *>(ichunkloader);
	AnvilChunkLoader *asyncAnvilLoader = dynamic_cast<AnvilChunkLoader *>(ichunkloader);
	// The worker reads region files only through McRegionChunkLoader. With the
	// Anvil loader it runs generation alone (a nullptr region loader), and
	// requestChunkDetailed() checks the save first so nothing on disk is ever
	// handed to it. Release worlds are Anvil, so without this branch the
	// worker never existed and every chunk generated on the game thread.
	if (dynamic_cast<ChunkProviderGenerate *>(chunkProvider) != nullptr && worldObj != nullptr &&
		(asyncRegionLoader != nullptr || asyncAnvilLoader != nullptr))
	{
		asyncSavedChunkProbe = asyncRegionLoader == nullptr ? asyncAnvilLoader : nullptr;
		#if PLATFORM_PC_LEGACY || PLATFORM_WII
		// The worker only builds terrain/cave buffers. Structure discovery,
		// decoration, Chunk construction, lighting and publication stay on the
		// game thread: the per-biome BiomeDecorator and the chunk-local
		// decoration scope on World are shared state, and a worker running
		// provideChunk() to completion raced the game thread's own decoration
		// ("Already decorating!!" on Wii).
		asyncGenerationScheduler = new ChunkGenerationScheduler(
			new ChunkProviderGenerate(
				worldObj, worldObj->getRandomSeed(), false,
									  PLATFORM_ASYNC_ISOLATED_BIOME_SOURCE != 0),
									  asyncRegionLoader, worldObj);
		#else
		asyncGenerationScheduler = new ChunkGenerationScheduler(
			new ChunkProviderGenerate(worldObj, worldObj->getRandomSeed()),
																asyncRegionLoader, worldObj);
		#endif
		if (!asyncGenerationScheduler->start())
		{
			delete asyncGenerationScheduler;
			asyncGenerationScheduler = nullptr;
			asyncSavedChunkProbe = nullptr;
		}
	}
	#endif
}

ChunkProvider::~ChunkProvider()
{
	#if PLATFORM_ASYNC_CHUNK_GENERATION
	delete asyncGenerationScheduler;
	asyncGenerationScheduler = nullptr;
	#endif
	#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
	if (ChunkProviderGenerate *generator = dynamic_cast<ChunkProviderGenerate *>(chunkProvider))
		generator->cancelGenerationTask();
	generationQueue.clear();
	generationQueued.clear();
	#endif
	std::unordered_set<Chunk *> uniqueChunks;
	uniqueChunks.reserve(chunkMap.size());
	for (auto &entry : chunkMap)
	{
		Chunk *chunk = entry.second;
		if (chunk != nullptr && chunk != blankChunk)
			uniqueChunks.insert(chunk);
	}

	for (Chunk *chunk : uniqueChunks)
		delete chunk;

	chunkMap.clear();
	chunkList.clear();
	droppedChunksSet.clear();

	delete blankChunk;
	delete chunkLoader;
	delete chunkProvider;

	blankChunk = nullptr;
	chunkLoader = nullptr;
	chunkProvider = nullptr;
	worldObj = nullptr;
}

std::uint64_t ChunkProvider::chunkKey(int_t i, int_t j)
{
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 32)
	| static_cast<std::uint32_t>(j);
}

void ChunkProvider::markChunkTopologyChanged()
{
	++chunkTopologyVersion;
	if (chunkTopologyVersion == 0)
		++chunkTopologyVersion;
}

void ChunkProvider::setCurrentChunkOver(int_t i, int_t j)
{
	#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
	if (generationCenterInitialized)
	{
		const int_t deltaX = JavaArithmetic::intSub(i, curChunkX);
		const int_t deltaZ = JavaArithmetic::intSub(j, curChunkZ);
		if (deltaX != 0 || deltaZ != 0)
		{
			generationMoveX = deltaX > 0 ? 1 : (deltaX < 0 ? -1 : 0);
			generationMoveZ = deltaZ > 0 ? 1 : (deltaZ < 0 ? -1 : 0);
		}
	}
	else
	{
		generationMoveX = 0;
		generationMoveZ = 0;
		generationCenterInitialized = true;
	}
	#endif
	curChunkX = i;
	curChunkZ = j;

	// Propagate the player-centred unload origin to the inner provider. Without
	// this the inner ChunkProviderLoadOrGenerate keeps curChunkX/Z at its initial
	// (spawn) value, so its distance-based unload100OldestChunks() never evicts the
	// chunks it generates around the moving player -- they accumulate and leak RAM
	// as you walk (the 32 MB "OOM after ~30 s of moving"). Both cache tiers must be
	// centred on the player. dynamic_cast because setCurrentChunkOver is not on the
	// IChunkProvider interface.
	if (ChunkProviderLoadOrGenerate *inner = dynamic_cast<ChunkProviderLoadOrGenerate *>(chunkProvider))
		inner->setCurrentChunkOver(i, j);
	else if (ChunkProvider *inner2 = dynamic_cast<ChunkProvider *>(chunkProvider))
		inner2->setCurrentChunkOver(i, j);
}

#if PLATFORM_BOUNDED_WORLD || PLATFORM_ASYNC_CHUNK_GENERATION
void ChunkProvider::notifyChunkPublished(Chunk *chunk)
{
	if (worldObj == nullptr || chunk == nullptr || chunk == blankChunk)
		return;

	// A console renderer at the edge of the sliding world cache is allowed to
	// build against EmptyChunk for source chunks outside the current load radius.
	// If that chunk later becomes resident, its completed empty column is stale.
	// Chunk publication is the authoritative transition, so invalidate that
	// column here. Use the interior range because RenderGlobal expands dirty
	// ranges by one block; this reaches the chunk bounds without rebuilding all
	// eight horizontal neighbours for every streamed chunk. Active neighbouring
	// builds detect source availability changes themselves in WorldRenderer.
	const int_t minX = JavaArithmetic::intMul(chunk->xPosition, 16);
	const int_t minZ = JavaArithmetic::intMul(chunk->zPosition, 16);
	worldObj->markBlocksDirty(minX + 1, 1, minZ + 1,
							  minX + 14, 126, minZ + 14);
	#if PLATFORM_PS2
	worldObj->notifyChunkPublishedForRender(chunk->xPosition, chunk->zPosition);
	#endif
}
#endif

void ChunkProvider::setChunkLoadRadius(int_t radius)
{
	// Use a slightly larger RAM cache than the strict visible radius.  This avoids
	// unloading/reloading the same chunks when the player moves a few blocks,
	// without going back to the old fixed 1024-slot cache.
	#if PLATFORM_BOUNDED_WORLD
	(void)radius;
	ISaveHandler *saveHandler = worldObj != nullptr ? worldObj->getSaveHandler() : nullptr;
	const ChunkMemoryPolicy::RetentionPolicy policy =
	ChunkMemoryPolicy::retentionPolicy(saveHandler != nullptr && saveHandler->isReadOnly());
	chunkLoadRadius = policy.loadRadius;
	chunkUnloadRadius = policy.unloadRadius;
	#elif PLATFORM_PC_LEGACY
	chunkLoadRadius = clampInt(radius, 2, PLATFORM_VISIBLE_CHUNK_RADIUS);
	chunkUnloadRadius = clampInt(chunkLoadRadius + 1, chunkLoadRadius, PLATFORM_VISIBLE_CHUNK_RADIUS + 1);
	#else
	chunkLoadRadius = clampInt(radius, 2, 15);
	chunkUnloadRadius = clampInt(chunkLoadRadius + CACHE_RADIUS_MARGIN, chunkLoadRadius, 15);
	#endif
}

void ChunkProvider::setChunkLoadRadiusFromRenderDistance(int_t renderDistance)
{
	#if PLATFORM_BOUNDED_WORLD
	(void)renderDistance;
	setChunkLoadRadius(0);
	#elif PLATFORM_PC_LEGACY
	(void)renderDistance;
	const int_t radius = Config::limit(
		Config::getRenderDistanceFine() / 16, 2, PLATFORM_VISIBLE_CHUNK_RADIUS);
	setChunkLoadRadius(radius);
	#else
	renderDistance &= 3;
	int_t blocks = 64 << (3 - renderDistance);
	if (blocks > 400)
		blocks = 400;

	const int_t renderChunksWide = blocks / 16 + 1;
	setChunkLoadRadius(renderChunksWide / 2 + 2);
	#endif
}

bool ChunkProvider::canChunkExist(int_t i, int_t j) const
{
	const int_t minX = JavaArithmetic::intSub(curChunkX, chunkLoadRadius);
	const int_t minZ = JavaArithmetic::intSub(curChunkZ, chunkLoadRadius);
	const int_t maxX = JavaArithmetic::intAdd(curChunkX, chunkLoadRadius);
	const int_t maxZ = JavaArithmetic::intAdd(curChunkZ, chunkLoadRadius);
	if (i >= minX && j >= minZ && i <= maxX && j <= maxZ)
		return true;
	if (worldObj != nullptr && worldObj->isChunkResident(i, j))
		return true;
	#if PLATFORM_ENTITY_CHUNK_RETENTION
	return worldObj != nullptr && worldObj->isChunkRetainedByEntity(i, j);
	#else
	return false;
	#endif
}

long_t ChunkProvider::currentWorldTime() const
{
	return worldObj != nullptr ? worldObj->getWorldTime() : 0LL;
}

bool ChunkProvider::isOutsideUnloadRadius(int_t i, int_t j) const
{
	if (worldObj != nullptr && worldObj->isChunkResident(i, j))
		return false;
	#if PLATFORM_ENTITY_CHUNK_RETENTION
	if (worldObj != nullptr && worldObj->isChunkRetainedByEntity(i, j))
		return false;
	#endif
	const long_t dx = static_cast<long_t>(i) - static_cast<long_t>(curChunkX);
	const long_t dz = static_cast<long_t>(j) - static_cast<long_t>(curChunkZ);
	const long_t radius = static_cast<long_t>(chunkUnloadRadius);
	return dx < -radius || dz < -radius || dx > radius || dz > radius;
}

bool ChunkProvider::chunkExists(int_t i, int_t j)
{
	return chunkMap.count(chunkKey(i, j)) != 0;
}

Chunk *ChunkProvider::getChunkIfExists(int_t i, int_t j)
{
	const std::uint64_t key = chunkKey(i, j);
	auto it = chunkMap.find(key);
	if (it == chunkMap.end())
		return nullptr;

	#if PLATFORM_BOUNDED_WORLD
	if (worldObj != nullptr && !worldObj->findingSpawnPoint && !canChunkExist(i, j))
		return blankChunk;
	#endif
	if (lastChunk != nullptr && i == lastChunkX && j == lastChunkZ)
		return lastChunk;

	Chunk *chunk = it->second;
	if (chunk != nullptr)
		chunk->lastAccessTick = currentWorldTime();
	if (chunk != nullptr && chunk != blankChunk)
	{
		lastChunk = chunk;
		lastChunkX = i;
		lastChunkZ = j;
	}
	return chunk;
}

#if PLATFORM_ASYNC_CHUNK_GENERATION
ChunkProvider::ChunkRequestStatus ChunkProvider::requestChunkDetailed(int_t i, int_t j)
{
	if (asyncGenerationScheduler == nullptr || !asyncGenerationScheduler->active())
		return ChunkRequestStatus::Inactive;
	if (worldObj != nullptr && !worldObj->findingSpawnPoint && !canChunkExist(i, j))
		return ChunkRequestStatus::OutOfRange;
	if (chunkMap.count(chunkKey(i, j)) != 0)
		return ChunkRequestStatus::AlreadyLoaded;
	if (asyncSavedChunkProbe != nullptr && asyncSavedChunkProbe->isChunkSaved(i, j))
		return ChunkRequestStatus::SavedOnDisk;

	switch (asyncGenerationScheduler->requestDetailed(i, j, PLATFORM_ASYNC_GENERATION_QUEUE_LIMIT))
	{
		case ChunkGenerationScheduler::RequestStatus::Accepted:
			return ChunkRequestStatus::Accepted;
		case ChunkGenerationScheduler::RequestStatus::AlreadyQueued:
			return ChunkRequestStatus::AlreadyQueued;
		case ChunkGenerationScheduler::RequestStatus::QueueFull:
			return ChunkRequestStatus::QueueFull;
		case ChunkGenerationScheduler::RequestStatus::Inactive:
		default:
			return ChunkRequestStatus::Inactive;
	}
}

bool ChunkProvider::requestChunk(int_t i, int_t j)
{
	return requestChunkDetailed(i, j) == ChunkRequestStatus::Accepted;
}

void ChunkProvider::serviceAsyncChunkStreaming()
{
	if (asyncGenerationScheduler == nullptr || !asyncGenerationScheduler->active())
		return;

	asyncGenerationScheduler->setFocus(curChunkX, curChunkZ);
	if (PLATFORM_ASYNC_GENERATION_PUBLISH_PER_FRAME > 0)
		drainAsyncGeneratedChunks(PLATFORM_ASYNC_GENERATION_PUBLISH_PER_FRAME);
	if (PLATFORM_ASYNC_GENERATION_REQUESTS_PER_FRAME > 0)
		drainAsyncGenerationRequests(PLATFORM_ASYNC_GENERATION_REQUESTS_PER_FRAME);
}

bool ChunkProvider::acceptAsyncGenerationCoordinate(void* context, int_t i, int_t j)
{
	ChunkProvider* self = static_cast<ChunkProvider*>(context);
	if (self == nullptr)
		return false;
	if (self->chunkMap.count(chunkKey(i, j)) != 0)
		return false;
	return self->worldObj == nullptr || self->worldObj->findingSpawnPoint || self->canChunkExist(i, j);
}

bool ChunkProvider::drainAsyncGenerationRequests(int_t budget)
{
	return asyncGenerationScheduler != nullptr &&
	asyncGenerationScheduler->dispatch(budget, &ChunkProvider::acceptAsyncGenerationCoordinate, this);
}

bool ChunkProvider::drainAsyncGeneratedChunks(int_t budget)
{
	if (asyncGenerationScheduler == nullptr || budget <= 0)
		return false;

	bool published = false;
	for (int_t n = 0; n < budget; ++n)
	{
		ChunkGenerationScheduler::Result result;
		if (!asyncGenerationScheduler->popResult(result))
			break;

		const std::uint64_t key = chunkKey(result.x, result.z);
		const bool wanted = chunkMap.count(key) == 0
		&& (worldObj == nullptr || worldObj->findingSpawnPoint || canChunkExist(result.x, result.z));

		Chunk *chunk = nullptr;
		if (wanted)
		{
			switch (result.kind)
			{
				case ChunkGenerationScheduler::ResultKind::LoadedData:
				{
					McRegionChunkLoader* regionLoader = dynamic_cast<McRegionChunkLoader*>(chunkLoader);
					if (regionLoader != nullptr)
					{
						ChunkLoadStatus loadStatus = ChunkLoadStatus::ReadError;
						chunk = regionLoader->loadChunkFromData(worldObj, result.x, result.z, result.data, &loadStatus);
						if (chunk != nullptr)
							chunk->lastSaveTime = currentWorldTime();
						else if (loadStatus == ChunkLoadStatus::ReadError)
							chunk = blankChunk;
					}
					break;
				}
				case ChunkGenerationScheduler::ResultKind::LoadedChunk:
					chunk = result.chunk;
					result.chunk = nullptr;
					McRegionChunkLoader::attachChunkEntities(worldObj, chunk, result.nbt.get());
					chunk->lastSaveTime = currentWorldTime();
					break;
				case ChunkGenerationScheduler::ResultKind::GeneratedData:
				{
					ChunkProviderGenerate *generator = dynamic_cast<ChunkProviderGenerate *>(chunkProvider);
					if (generator != nullptr)
						chunk = generator->finishAsyncChunkData(result.x, result.z, result.data);
					break;
				}
				case ChunkGenerationScheduler::ResultKind::Generated:
					chunk = result.chunk;
					result.chunk = nullptr;
					break;
				case ChunkGenerationScheduler::ResultKind::ReadError:
					chunk = blankChunk;
					break;
			}
		}

		if (chunk != nullptr)
		{
			published = true;
			chunkMap[key] = chunk;
			markChunkTopologyChanged();
			chunkList.push_back(chunk);
			chunk->lastAccessTick = currentWorldTime();
			if (chunk != blankChunk)
			{
				chunk->onChunkLoadData();
				chunk->onChunkLoad();
				notifyChunkPublished(chunk);
				#if PLATFORM_DEFERRED_POPULATE
				const int_t westX = JavaArithmetic::intSub(result.x, 1);
				const int_t northZ = JavaArithmetic::intSub(result.z, 1);
				enqueuePopulate(result.x, result.z);
				enqueuePopulate(westX, result.z);
				enqueuePopulate(result.x, northZ);
				enqueuePopulate(westX, northZ);
				#endif
			}
		}

		delete result.chunk;
		asyncGenerationScheduler->complete(result.x, result.z);
	}
	return published;
}
#endif

void ChunkProvider::publishPreparedChunk(int_t i, int_t j, Chunk *chunk)
{
	if (chunk == nullptr)
		return;

	const std::uint64_t key = chunkKey(i, j);
	chunkMap[key] = chunk;
	markChunkTopologyChanged();
	chunkList.push_back(chunk);
	chunk->lastAccessTick = currentWorldTime();
	chunk->onChunkLoadData();
	chunk->onChunkLoad();

	#if PLATFORM_BOUNDED_WORLD
	if (chunk != blankChunk)
		notifyChunkPublished(chunk);
	#endif

	if (chunk == blankChunk)
		return;

	const int_t eastX = JavaArithmetic::intAdd(i, 1);
	const int_t westX = JavaArithmetic::intSub(i, 1);
	const int_t southZ = JavaArithmetic::intAdd(j, 1);
	const int_t northZ = JavaArithmetic::intSub(j, 1);
	#if PLATFORM_DEFERRED_POPULATE
	const bool deferPopulate =
	#if PLATFORM_PC_LEGACY
	worldObj == nullptr || !worldObj->findingSpawnPoint;
	#else
	true;
	#endif
	if (deferPopulate)
	{
		enqueuePopulate(i, j);
		enqueuePopulate(westX, j);
		enqueuePopulate(i, northZ);
		enqueuePopulate(westX, northZ);
		return;
	}
	#endif

	if (!chunk->isTerrainPopulated
		&& chunkExists(eastX, southZ)
		&& chunkExists(i, southZ)
		&& chunkExists(eastX, j))
	{
		populate(this, i, j);
	}
	if (chunkExists(westX, j) && !provideChunk(westX, j)->isTerrainPopulated
		&& chunkExists(westX, southZ)
		&& chunkExists(i, southZ)
		&& chunkExists(westX, j))
	{
		populate(this, westX, j);
	}
	if (chunkExists(i, northZ) && !provideChunk(i, northZ)->isTerrainPopulated
		&& chunkExists(eastX, northZ)
		&& chunkExists(i, northZ)
		&& chunkExists(eastX, j))
	{
		populate(this, i, northZ);
	}
	if (chunkExists(westX, northZ) && !provideChunk(westX, northZ)->isTerrainPopulated
		&& chunkExists(westX, northZ)
		&& chunkExists(i, northZ)
		&& chunkExists(westX, j))
	{
		populate(this, westX, northZ);
	}
}

Chunk *ChunkProvider::prepareChunk(int_t i, int_t j)
{
	return prepareChunkInternal(i, j, false);
}

Chunk *ChunkProvider::prepareChunkInternal(int_t i, int_t j, bool deferGeneration)
{
	#if !PLATFORM_INCREMENTAL_CHUNK_GENERATION
	(void)deferGeneration;
	#endif
	#if PLATFORM_BOUNDED_WORLD
	if (worldObj != nullptr && !worldObj->findingSpawnPoint && !canChunkExist(i, j))
		return blankChunk;
	#endif
	const std::uint64_t key = chunkKey(i, j);
	droppedChunksSet.erase(key);

	auto it = chunkMap.find(key);
	Chunk *chunk = (it != chunkMap.end()) ? it->second : nullptr;
	if (chunk != nullptr)
	{
		chunk->lastAccessTick = currentWorldTime();
		return chunk;
	}

	WORLD_LOAD_STAGE("prepareChunk");
	bool readFailed = false;
	#if PLATFORM_PROFILE_STREAMING
	const long_t chunkLoadStartNs = System::nanoTime();
	#endif
	WorldLoadTrace::step("loadChunkFromFile");
	chunk = loadChunkFromFile(i, j, readFailed);
	#if PLATFORM_PROFILE_STREAMING
	platformProfileChunkLoad(System::nanoTime() - chunkLoadStartNs);
	#endif
	if (chunk == nullptr && readFailed)
	{
		MC_LOG_ERROR("chunk", "ChunkProvider: refusing to regenerate unreadable chunk %d,%d\n", i, j);
		chunk = blankChunk;
	}
	else if (chunk == nullptr)
	{
		#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
		if (deferGeneration)
		{
			enqueueGeneration(i, j);
			return blankChunk;
		}
		#endif
		if (chunkProvider == nullptr)
		{
			chunk = blankChunk;
		}
		else
		{
			#if PLATFORM_PROFILE_STREAMING
			const long_t generateStartNs = System::nanoTime();
			#endif
			WorldLoadTrace::step("generate");
			chunk = chunkProvider->provideChunk(i, j);
			#if PLATFORM_PROFILE_STREAMING
			platformProfileGenerate(System::nanoTime() - generateStartNs);
			#endif
		}
	}
	if (chunk == nullptr)
		chunk = blankChunk;

	publishPreparedChunk(i, j, chunk);
	return chunk;
}

#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
void ChunkProvider::enqueueGeneration(int_t i, int_t j)
{
	const std::uint64_t key = chunkKey(i, j);
	if (generationQueued.insert(key).second)
		generationQueue.emplace_back(i, j);
}

void ChunkProvider::cancelQueuedGeneration(int_t i, int_t j)
{
	const std::uint64_t key = chunkKey(i, j);
	generationQueued.erase(key);
	generationQueue.erase(
		std::remove_if(generationQueue.begin(), generationQueue.end(),
					   [i, j](const std::pair<int_t, int_t> &coord)
					   {
						   return coord.first == i && coord.second == j;
					   }),
					   generationQueue.end());

	ChunkProviderGenerate *generator = dynamic_cast<ChunkProviderGenerate *>(chunkProvider);
	if (generator != nullptr && generator->hasGenerationTask()
		&& generator->generationTaskX() == i && generator->generationTaskZ() == j)
	{
		generator->cancelGenerationTask();
	}
}

int_t ChunkProvider::countLoadedGenerationNeighbours(int_t i, int_t j) const
{
	int_t loaded = 0;
	for (int_t dx = -1; dx <= 1; ++dx)
	{
		for (int_t dz = -1; dz <= 1; ++dz)
		{
			if (dx == 0 && dz == 0)
				continue;

			const int_t neighbourX = JavaArithmetic::intAdd(i, dx);
			const int_t neighbourZ = JavaArithmetic::intAdd(j, dz);
			auto it = chunkMap.find(chunkKey(neighbourX, neighbourZ));
			if (it != chunkMap.end() && it->second != nullptr && it->second != blankChunk)
				++loaded;
		}
	}
	return loaded;
}

std::deque<std::pair<int_t, int_t>>::iterator ChunkProvider::selectNextGenerationCoordinate()
{
	auto best = generationQueue.begin();
	if (best == generationQueue.end())
		return best;

	auto chebyshevDistance = [this](const std::pair<int_t, int_t> &coord)
	{
		long_t dx = static_cast<long_t>(coord.first) - static_cast<long_t>(curChunkX);
		long_t dz = static_cast<long_t>(coord.second) - static_cast<long_t>(curChunkZ);
		if (dx < 0) dx = -dx;
		if (dz < 0) dz = -dz;
		return dx > dz ? dx : dz;
	};

	auto movementAlignment = [this](const std::pair<int_t, int_t> &coord)
	{
		const long_t dx = static_cast<long_t>(coord.first) - static_cast<long_t>(curChunkX);
		const long_t dz = static_cast<long_t>(coord.second) - static_cast<long_t>(curChunkZ);
		return dx * static_cast<long_t>(generationMoveX)
		+ dz * static_cast<long_t>(generationMoveZ);
	};

	long_t bestDistance = chebyshevDistance(*best);
	for (auto it = generationQueue.begin() + 1; it != generationQueue.end(); ++it)
	{
		const long_t distance = chebyshevDistance(*it);
		if (distance < bestDistance)
		{
			best = it;
			bestDistance = distance;
		}
	}

	int_t bestNeighbours = countLoadedGenerationNeighbours(best->first, best->second);
	long_t bestAlignment = movementAlignment(*best);
	for (auto it = generationQueue.begin(); it != generationQueue.end(); ++it)
	{
		if (it == best || chebyshevDistance(*it) != bestDistance)
			continue;

		const int_t neighbours = countLoadedGenerationNeighbours(it->first, it->second);
		if (neighbours < bestNeighbours)
			continue;

		const long_t alignment = movementAlignment(*it);
		if (neighbours > bestNeighbours || alignment > bestAlignment)
		{
			best = it;
			bestNeighbours = neighbours;
			bestAlignment = alignment;
		}
	}

	return best;
}

bool ChunkProvider::drainPendingGeneration(int_t stepBudget, bool &publishedChunk)
{
	return drainPendingGeneration(stepBudget,
								  static_cast<long_t>(PLATFORM_GENERATION_BUDGET_US) * 1000LL, publishedChunk);
}

void ChunkProvider::serviceFrameGeneration()
{
	if (PLATFORM_GENERATION_STEPS_PER_FRAME <= 0)
		return;
	bool publishedChunk = false;
	drainPendingGeneration(PLATFORM_GENERATION_STEPS_PER_FRAME,
						   static_cast<long_t>(PLATFORM_GENERATION_FRAME_BUDGET_US) * 1000LL, publishedChunk);
}

bool ChunkProvider::drainPendingGeneration(int_t stepBudget, long_t budgetNs, bool &publishedChunk)
{
	publishedChunk = false;
	if (stepBudget <= 0)
		return false;

	ChunkProviderGenerate *generator = dynamic_cast<ChunkProviderGenerate *>(chunkProvider);
	if (generator == nullptr)
		return false;

	bool didWork = false;
	int_t steps = 0;

	PlatformStreamingFrameBudgetScope frameBudgetScope;
	budgetNs = PlatformStreamingFrameBudget::clampUs(budgetNs / 1000LL) * 1000LL;
	const long_t budgetStartNs = budgetNs > 0 ? System::nanoTime() : 0;

	while (steps < stepBudget)
	{
		if (!generator->hasGenerationTask())
		{
			bool started = false;
			while (!generationQueue.empty())
			{
				auto coordIt = selectNextGenerationCoordinate();
				if (coordIt == generationQueue.end())
					break;
				const std::pair<int_t, int_t> coord = *coordIt;
				generationQueue.erase(coordIt);
				const std::uint64_t key = chunkKey(coord.first, coord.second);
				if (generationQueued.count(key) == 0)
					continue;

				const bool wanted = chunkMap.count(key) == 0
				&& (worldObj == nullptr || worldObj->findingSpawnPoint
				|| canChunkExist(coord.first, coord.second));
				if (!wanted)
				{
					generationQueued.erase(key);
					continue;
				}

				started = generator->beginGenerationTask(coord.first, coord.second);
				if (started)
					break;
				generationQueue.emplace_back(coord);
				return didWork;
			}
			if (!started)
				break;
		}

		const int_t taskX = generator->generationTaskX();
		const int_t taskZ = generator->generationTaskZ();
		const std::uint64_t key = chunkKey(taskX, taskZ);
		const bool wanted = chunkMap.count(key) == 0
		&& (worldObj == nullptr || worldObj->findingSpawnPoint || canChunkExist(taskX, taskZ));
		if (!wanted)
		{
			generator->cancelGenerationTask();
			generationQueued.erase(key);
			continue;
		}

		if (!generator->advanceGenerationTask())
			break;
		didWork = true;
		++steps;

		Chunk *completed = generator->takeGeneratedChunk();
		if (completed != nullptr)
		{
			generationQueued.erase(key);
			const bool stillWanted = chunkMap.count(key) == 0
			&& (worldObj == nullptr || worldObj->findingSpawnPoint || canChunkExist(taskX, taskZ));
			if (stillWanted)
			{
				publishPreparedChunk(taskX, taskZ, completed);
				publishedChunk = true;
			}
			else
			{
				delete completed;
			}
		}

		if (budgetNs > 0 && System::nanoTime() - budgetStartNs >= budgetNs)
			break;
	}
	return didWork;
}

bool ChunkProvider::isChunkGenerationPending(int_t i, int_t j) const
{
	return generationQueued.count(chunkKey(i, j)) != 0;
}
#endif

Chunk *ChunkProvider::provideChunk(int_t i, int_t j)
{
	#if PLATFORM_BOUNDED_WORLD
	if (worldObj != nullptr && !worldObj->findingSpawnPoint && !canChunkExist(i, j))
		return blankChunk;
	#endif
	if (lastChunk != nullptr && i == lastChunkX && j == lastChunkZ)
		return lastChunk;

	const std::uint64_t key = chunkKey(i, j);
	auto it = chunkMap.find(key);
	if (it == chunkMap.end())
	{
		#if PLATFORM_ASYNC_CHUNK_GENERATION && PLATFORM_PC_LEGACY
		if (worldObj == nullptr || !worldObj->findingSpawnPoint)
		{
			long_t dcx = static_cast<long_t>(i) - static_cast<long_t>(curChunkX);
			long_t dcz = static_cast<long_t>(j) - static_cast<long_t>(curChunkZ);
			if (dcx < 0) dcx = -dcx;
			if (dcz < 0) dcz = -dcz;
			const long_t cheb = dcx > dcz ? dcx : dcz;
			const bool critical = cheb <= PLATFORM_GENERATE_SYNC_RADIUS;
			if (!critical && asyncGenerationScheduler != nullptr && asyncGenerationScheduler->active())
			{
				const ChunkRequestStatus requestStatus = requestChunkDetailed(i, j);
				if (requestStatus == ChunkRequestStatus::Accepted ||
					requestStatus == ChunkRequestStatus::AlreadyQueued ||
					requestStatus == ChunkRequestStatus::QueueFull)
					return blankChunk;
			}
		}
		#endif
		#if PLATFORM_BOUNDED_WORLD && PLATFORM_GENERATE_CHUNKS_PER_TICK > 0
		if (worldObj == nullptr || !worldObj->findingSpawnPoint)
		{
			long_t dcx = static_cast<long_t>(i) - static_cast<long_t>(curChunkX);
			long_t dcz = static_cast<long_t>(j) - static_cast<long_t>(curChunkZ);
			if (dcx < 0) dcx = -dcx;
			if (dcz < 0) dcz = -dcz;
			const long_t cheb = dcx > dcz ? dcx : dcz;
			bool critical = cheb <= PLATFORM_GENERATE_SYNC_RADIUS;
			#if PLATFORM_ENTITY_CHUNK_RETENTION
			critical = critical || (worldObj != nullptr && worldObj->isChunkRequiredByRetainedEntity(i, j));
			#endif
			#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
			ChunkProviderGenerate *incrementalGenerator = dynamic_cast<ChunkProviderGenerate *>(chunkProvider);
			if (incrementalGenerator != nullptr)
			{
				if (critical)
				{
					cancelQueuedGeneration(i, j);
				}
				else
				{
					if (generationQueued.count(key) != 0)
						return blankChunk;
					return prepareChunkInternal(i, j, true);
				}
			}
			#endif
			#if PLATFORM_ASYNC_CHUNK_GENERATION
			if (!critical && asyncGenerationScheduler != nullptr && asyncGenerationScheduler->active())
			{
				const ChunkRequestStatus requestStatus = requestChunkDetailed(i, j);
				if (requestStatus == ChunkRequestStatus::Accepted ||
					requestStatus == ChunkRequestStatus::AlreadyQueued)
					return blankChunk;
			}
			#endif
			if (!critical && genChunksThisTick >= PLATFORM_GENERATE_CHUNKS_PER_TICK)
				return blankChunk;
			genChunksThisTick++;
		}
		#endif
		return prepareChunk(i, j);
	}

	if (it->second != nullptr)
		it->second->lastAccessTick = currentWorldTime();
	if (it->second != nullptr && it->second != blankChunk)
	{
		lastChunk  = it->second;
		lastChunkX = i;
		lastChunkZ = j;
	}
	return it->second;
}

Chunk *ChunkProvider::loadChunkFromFile(int_t i, int_t j, bool &readFailed)
{
	readFailed = false;
	if (chunkLoader == nullptr) return nullptr;
	try
	{
		ChunkLoadStatus status = ChunkLoadStatus::Missing;
		Chunk *chunk = chunkLoader->loadChunk(worldObj, i, j, &status);
		readFailed = status == ChunkLoadStatus::ReadError;
		if (chunk != nullptr)
			chunk->lastSaveTime = worldObj->getWorldTime();
		return chunk;
	}
	catch (...)
	{
		readFailed = true;
		MC_LOG_ERROR("chunk", "ChunkProvider::loadChunkFromFile - exception loading %d,%d\n", i, j);
	}
	return nullptr;
}

void ChunkProvider::saveExtraChunkData(Chunk *chunk)
{
	if (chunkLoader == nullptr || chunk == nullptr || chunk == blankChunk) return;
	try
	{
		chunkLoader->saveExtraChunkData(worldObj, chunk);
	}
	catch (...)
	{
		MC_LOG_ERROR("chunk", "ChunkProvider::saveExtraChunkData - exception\n");
	}
}

void ChunkProvider::saveChunkToFile(Chunk *chunk)
{
	if (chunkLoader == nullptr || chunk == nullptr || chunk == blankChunk) return;
	try
	{
		chunk->lastSaveTime = worldObj->getWorldTime();
		chunkLoader->saveChunk(worldObj, chunk);
	}
	catch (...)
	{
		MC_LOG_ERROR("chunk", "ChunkProvider::saveChunkToFile - exception\n");
	}
}

void ChunkProvider::unloadChunk(std::uint64_t key, Chunk *chunk)
{
	if (chunk == nullptr || chunk == blankChunk)
		return;

	// Drop the single-entry cache if it points at the chunk being freed.
	if (lastChunk == chunk)
		lastChunk = nullptr;

	chunkMap.erase(key);

	#if PLATFORM_SAVE_RUNTIME_CHUNK_EDITS_ON_UNLOAD
	if (!chunk->neverSave && chunk->isRuntimeSaveRequired())
	{
		#if PLATFORM_PROFILE_STREAMING
		const long_t unloadSaveStartNs = System::nanoTime();
		#endif
		saveChunkToFile(chunk);
		chunk->isModified = false;
		chunk->clearRuntimeSaveRequired();
		saveExtraChunkData(chunk);
		#if PLATFORM_PROFILE_STREAMING
		platformProfileUnloadSave(System::nanoTime() - unloadSaveStartNs);
		#endif
	}
	#elif !PLATFORM_CONSOLE_LOW
	if (!chunk->neverSave && chunk->needsSaving(false))
	{
		#if PLATFORM_PROFILE_STREAMING
		const long_t unloadSaveStartNs = System::nanoTime();
		#endif
		saveChunkToFile(chunk);
		chunk->isModified = false;
		chunk->clearRuntimeSaveRequired();
		saveExtraChunkData(chunk);
		#if PLATFORM_PROFILE_STREAMING
		platformProfileUnloadSave(System::nanoTime() - unloadSaveStartNs);
		#endif
	}
	#endif

	chunk->onChunkUnload();
	delete chunk;
}

bool ChunkProvider::isChunkPopulationPending(int_t i, int_t j) const
{
	#if PLATFORM_DEFERRED_POPULATE
	return populateQueued.find(chunkKey(i, j)) != populateQueued.end();
	#else
	(void)i;
	(void)j;
	return false;
	#endif
}

void ChunkProvider::populate(IChunkProvider *ichunkprovider, int_t i, int_t j)
{
	#if PLATFORM_DEFERRED_POPULATE
	while (!populateDeferredStep(i, j))
	{
	}
	#else
	Chunk *chunk = provideChunk(i, j);
	if (chunk != nullptr && chunk != blankChunk && !chunk->isTerrainPopulated)
	{
		chunk->isTerrainPopulated = true;
		if (chunkProvider != nullptr)
		{
			chunkProvider->populate(ichunkprovider, i, j);
			chunk->setChunkModified();
		}
	}
	#endif
}

#if PLATFORM_DEFERRED_POPULATE
bool ChunkProvider::populateDeferredBatch(int_t i, int_t j, int_t maxSteps, long_t deadlineNs, int_t &stepsRun)
{
	stepsRun = 0;
	Chunk *chunk = provideChunk(i, j);
	if (chunk == nullptr || chunk == blankChunk || chunk->isTerrainPopulated)
		return true;
	if (chunkProvider == nullptr)
	{
		chunk->isTerrainPopulated = true;
		stepsRun = 1;
		return true;
	}

	Chunk *populationChunks[POPULATION_FOOTPRINT_AXIS * POPULATION_FOOTPRINT_AXIS] = {};
	std::uint32_t before[POPULATION_FOOTPRINT_AXIS * POPULATION_FOOTPRINT_AXIS]
	[POPULATION_SECTION_COUNT] = {};
	for (int_t dz = 0; dz < POPULATION_FOOTPRINT_AXIS; ++dz)
	{
		for (int_t dx = 0; dx < POPULATION_FOOTPRINT_AXIS; ++dx)
		{
			const int_t footprintIndex = dz * POPULATION_FOOTPRINT_AXIS + dx;
			Chunk *footprintChunk = getLoadedChunk(JavaArithmetic::intAdd(i, dx), JavaArithmetic::intAdd(j, dz));
			populationChunks[footprintIndex] = footprintChunk;
			for (int_t sectionY = 0; sectionY < POPULATION_SECTION_COUNT; ++sectionY)
			{
				before[footprintIndex][sectionY] = footprintChunk != nullptr
				? footprintChunk->getBlockSectionRevision(sectionY)
				: 0u;
			}
		}
	}

	if (worldObj != nullptr)
		worldObj->beginPopulationFastPath(i, j);

	bool complete = false;
	try
	{
		while (stepsRun < maxSteps && !complete)
		{
			complete = chunkProvider->populateStep(this, i, j);
			++stepsRun;
			if (deadlineNs > 0 && System::nanoTime() >= deadlineNs)
				break;
		}
	}
	catch (...)
	{
		if (worldObj != nullptr)
			worldObj->endPopulationFastPath();
		throw;
	}

	if (worldObj != nullptr)
		worldObj->endPopulationFastPath();

	if (worldObj != nullptr)
	{
		for (int_t footprintIndex = 0;
			 footprintIndex < POPULATION_FOOTPRINT_AXIS * POPULATION_FOOTPRINT_AXIS;
		++footprintIndex)
			 {
				 Chunk *footprintChunk = populationChunks[footprintIndex];
				 if (footprintChunk == nullptr || footprintChunk == blankChunk)
					 continue;

				 for (int_t sectionY = 0; sectionY < POPULATION_SECTION_COUNT; ++sectionY)
				 {
					 if (before[footprintIndex][sectionY] ==
						 footprintChunk->getBlockSectionRevision(sectionY))
						 continue;

					 const int_t minX = JavaArithmetic::intMul(footprintChunk->xPosition, 16);
					 const int_t minY = sectionY << 4;
					 const int_t minZ = JavaArithmetic::intMul(footprintChunk->zPosition, 16);
					 worldObj->markBlocksDirty(minX, minY, minZ,
											   JavaArithmetic::intAdd(minX, 15), minY + 15,
											   JavaArithmetic::intAdd(minZ, 15));
				 }
			 }
	}

	if (complete)
	{
		chunk->isTerrainPopulated = true;
		chunk->setChunkModified();
	}
	return complete;
}

bool ChunkProvider::populateDeferredStep(int_t i, int_t j)
{
	int_t stepsRun = 0;
	return populateDeferredBatch(i, j, 1, 0, stepsRun);
}

Chunk *ChunkProvider::getLoadedChunk(int_t i, int_t j)
{
	auto it = chunkMap.find(chunkKey(i, j));
	return it != chunkMap.end() ? it->second : nullptr;
}

bool ChunkProvider::canPopulateChunk(int_t i, int_t j)
{
	Chunk *c = getLoadedChunk(i, j);
	if (c == nullptr || c == blankChunk || c->isTerrainPopulated)
		return false;
	const int_t eastX = JavaArithmetic::intAdd(i, 1);
	const int_t southZ = JavaArithmetic::intAdd(j, 1);
	return chunkExists(eastX, j)
	&& chunkExists(i, southZ)
	&& chunkExists(eastX, southZ);
}

void ChunkProvider::enqueuePopulate(int_t i, int_t j)
{
	#if PLATFORM_POPULATE_CHUNKS_PER_TICK <= 0
	(void)i; (void)j;
	return;
	#else
	Chunk *c = getLoadedChunk(i, j);
	if (c == nullptr || c == blankChunk || c->isTerrainPopulated)
		return;
	const std::uint64_t key = chunkKey(i, j);
	if (populateQueued.insert(key).second)
		populateQueue.emplace_back(i, j);
	#endif
}

void ChunkProvider::drainPendingPopulate(int_t budget)
{
	if (budget <= 0)
		return;
	int_t scan = (int_t)populateQueue.size();
	int_t steps = 0;
	#if PLATFORM_POPULATE_BUDGET_US > 0
	PlatformStreamingFrameBudgetScope frameBudgetScope;
	const long_t budgetStartNs = System::nanoTime();
	const long_t budgetNs =
	PlatformStreamingFrameBudget::clampUs((long_t)PLATFORM_POPULATE_BUDGET_US) * 1000LL;
	const long_t deadlineNs = budgetStartNs + budgetNs;
	#else
	const long_t deadlineNs = 0;
	#endif
	while (steps < budget && scan-- > 0 && !populateQueue.empty())
	{
		const std::pair<int_t, int_t> coord = populateQueue.front();
		populateQueue.pop_front();
		const std::uint64_t key = chunkKey(coord.first, coord.second);

		if (!canPopulateChunk(coord.first, coord.second))
		{
			populateQueued.erase(key);
			continue;
		}

		int_t batchSteps = 0;
		const bool complete = populateDeferredBatch(
			coord.first, coord.second, budget - steps, deadlineNs, batchSteps);
		steps += batchSteps;
		if (!complete)
		{
			populateQueue.emplace_front(coord);
			scan++;
		}
		else
		{
			populateQueued.erase(key);
		}
		#if PLATFORM_POPULATE_BUDGET_US > 0
		if (System::nanoTime() - budgetStartNs >= budgetNs)
			break;
		#endif
		if (!complete)
			continue;
	}
}
#endif

bool ChunkProvider::saveChunks(bool flag, IProgressUpdate *iprogressupdate)
{
	int_t saved = 0;
	int_t totalToSave = 0;
	if (iprogressupdate != nullptr)
	{
		for (Chunk *chunk : chunkList)
		{
			if (chunk != nullptr && chunk != blankChunk && chunk->needsSaving(flag))
				totalToSave++;
		}
	}

	int_t progress = 0;
	for (Chunk *chunk : chunkList)
	{
		if (chunk == nullptr || chunk == blankChunk) continue;
		if (flag && !chunk->neverSave)
			saveExtraChunkData(chunk);
		if (!chunk->needsSaving(flag))
			continue;
		saveChunkToFile(chunk);
		chunk->isModified = false;
		chunk->clearRuntimeSaveRequired();
		++saved;

		if (flag && (saved % std::max<int_t>(1, PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT)) == 0)
			ThreadedFileIOBase::threadedIOInstance.waitForFinish();

		if (saved == PLATFORM_INCREMENTAL_CHUNK_SAVE_LIMIT && !flag)
			return false;
		if (iprogressupdate != nullptr && totalToSave > 0 && ++progress % 10 == 0)
			iprogressupdate->setLoadingProgress((progress * 100) / totalToSave);
	}
	if (flag)
	{
		if (chunkLoader == nullptr) return true;
		chunkLoader->saveExtraData();
	}
	return true;
}

bool ChunkProvider::unload100OldestChunks()
{
	#if PLATFORM_DEFERRED_POPULATE
	bool publishedThisTick = false;
	#endif
	#if PLATFORM_BOUNDED_WORLD
	genChunksThisTick = 0;
	#if PLATFORM_INCREMENTAL_CHUNK_GENERATION
	#if PLATFORM_PROFILE_STREAMING
	const long_t generationStartNs = System::nanoTime();
	#endif
	bool incrementalPublishedChunk = false;
	const bool incrementalGenerationWorked =
	drainPendingGeneration(PLATFORM_GENERATION_STEPS_PER_TICK, incrementalPublishedChunk);
	#if PLATFORM_PROFILE_STREAMING
	if (incrementalGenerationWorked)
		platformProfileGenerate(System::nanoTime() - generationStartNs);
	#endif
	#if PLATFORM_DEFERRED_POPULATE
	publishedThisTick = incrementalPublishedChunk;
	#endif
	#endif
	#endif
	#if PLATFORM_ASYNC_CHUNK_GENERATION
	const bool asyncPublishedChunk = drainAsyncGeneratedChunks(PLATFORM_ASYNC_GENERATION_PUBLISH_PER_TICK);
	drainAsyncGenerationRequests(PLATFORM_ASYNC_GENERATION_REQUESTS_PER_TICK);
	#if PLATFORM_DEFERRED_POPULATE
	publishedThisTick = publishedThisTick || asyncPublishedChunk;
	#endif
	#endif

	#if PLATFORM_DEFERRED_POPULATE
	{
		const int_t populateSteps = publishedThisTick
		? PLATFORM_POPULATE_STEPS_AFTER_PUBLISH
		: PLATFORM_POPULATE_STEPS_PER_TICK;
		#if PLATFORM_PROFILE_STREAMING
		const long_t populateStartNs = System::nanoTime();
		#endif
		drainPendingPopulate(populateSteps);
		#if PLATFORM_PROFILE_STREAMING
		platformProfilePopulate(System::nanoTime() - populateStartNs);
		#endif
	}
	#endif

	#if PLATFORM_PROFILE_STREAMING
	const long_t chunkEvictStartNs = System::nanoTime();
	#endif
	int_t unloaded = 0;
	ISaveHandler *saveHandler = worldObj != nullptr ? worldObj->getSaveHandler() : nullptr;
	const ChunkMemoryPolicy::RetentionPolicy retentionPolicy =
	ChunkMemoryPolicy::retentionPolicy(saveHandler != nullptr && saveHandler->isReadOnly());

	// Cleaning up the explicit exclusion list
	while (!droppedChunksSet.empty() && unloaded < retentionPolicy.maxUnloadsPerTick)
	{
		std::uint64_t key = *droppedChunksSet.begin();
		droppedChunksSet.erase(droppedChunksSet.begin());

		auto it = chunkMap.find(key);
		if (it == chunkMap.end())
			continue;

		Chunk *chunk = it->second;
		chunkMap.erase(it);
		markChunkTopologyChanged();
		chunkList.erase(std::remove(chunkList.begin(), chunkList.end(), chunk), chunkList.end());

		unloadChunk(key, chunk);
		unloaded++;
	}

	const size_t maxResidentChunks = (size_t)((chunkUnloadRadius * 2 + 1) * (chunkUnloadRadius * 2 + 1));
	size_t chunksOutsideRadius = 0;
	for (const auto &entry : chunkMap)
	{
		Chunk *chunk = entry.second;
		if (chunk != nullptr && chunk != blankChunk
			&& isOutsideUnloadRadius(chunk->xPosition, chunk->zPosition))
		{
			chunksOutsideRadius++;
		}
	}
	const bool emergency = chunkMap.size() > maxResidentChunks && chunksOutsideRadius > 0;
	const int_t unloadLimit = ChunkMemoryPolicy::unloadLimit(retentionPolicy, emergency);

	const long_t now = currentWorldTime();
	for (auto it = chunkMap.begin(); it != chunkMap.end() && unloaded < unloadLimit; )
	{
		Chunk *chunk = it->second;
		if (chunk == nullptr || chunk == blankChunk)
		{
			it = chunkMap.erase(it);
			markChunkTopologyChanged();
			chunkList.erase(std::remove(chunkList.begin(), chunkList.end(), chunk), chunkList.end());
			continue;
		}

		const long_t lastAccess = chunk->lastAccessTick;

		if (isOutsideUnloadRadius(chunk->xPosition, chunk->zPosition)
			&& (emergency || JavaArithmetic::longSub(now, lastAccess) >= retentionPolicy.minUnusedTicksBeforeUnload))
		{
			std::uint64_t key = it->first;

			it = chunkMap.erase(it);
			markChunkTopologyChanged();
			chunkList.erase(std::remove(chunkList.begin(), chunkList.end(), chunk), chunkList.end());

			unloadChunk(key, chunk);
			unloaded++;

			if (chunksOutsideRadius > 0)
				chunksOutsideRadius--;
			if (emergency && chunksOutsideRadius == 0)
				break;
		}
		else
		{
			++it;
		}
	}

	#if PLATFORM_PROFILE_STREAMING
	platformProfileChunkEvict(System::nanoTime() - chunkEvictStartNs);
	#endif
	if (chunkLoader != nullptr)
		chunkLoader->chunkTick();

	const bool childUnloaded = chunkProvider != nullptr && chunkProvider->unload100OldestChunks();
	return unloaded > 0 || childUnloaded;
}

bool ChunkProvider::canSave()
{
	return true;
}

jstring ChunkProvider::makeString()
{
	jstring result = "ServerChunkCache: " + String::fromInt((int_t)chunkMap.size())
	+ " Radius: " + String::fromInt(chunkLoadRadius)
	+ " UnloadRadius: " + String::fromInt(chunkUnloadRadius)
	+ " Drop: " + String::fromInt((int_t)droppedChunksSet.size());
	#if PLATFORM_ASYNC_CHUNK_GENERATION
	if (asyncGenerationScheduler != nullptr)
	{
		int_t pending = 0, completed = 0;
		asyncGenerationScheduler->queueSizes(pending, completed);
		result += " GenQ: " + String::fromInt(pending)
		+ " GenDone: " + String::fromInt(completed);
	}
	#endif
	return result;
}

std::vector<SpawnListEntry> *ChunkProvider::getPossibleCreatures(const EnumCreatureType &type, int_t x, int_t y, int_t z)
{
	return chunkProvider != nullptr ? chunkProvider->getPossibleCreatures(type, x, y, z) : nullptr;
}

ChunkPosition *ChunkProvider::findClosestStructure(World *world, const jstring &name, int_t x, int_t y, int_t z)
{
	return chunkProvider != nullptr ? chunkProvider->findClosestStructure(world, name, x, y, z) : nullptr;
}

void ChunkProvider::removeEntityFromLoadedChunks(Entity *entity)
{
	for (const auto &entry : chunkMap)
	{
		Chunk *chunk = entry.second;
		if (chunk != nullptr && chunk != blankChunk)
			chunk->removeEntityFromAllSections(entity);
	}
}
