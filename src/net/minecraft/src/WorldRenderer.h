#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "java/Type.h"

#include "platform/RenderTerrainAPI.h"
#include "platform/PlatformConfig.h"
#include "Tessellator.h"

class World;
class TileEntity;
class ICamera;
class Entity;
class AxisAlignedBB;

// net.minecraft.src.WorldRenderer
class WorldRenderer
{
public:
	WorldRenderer(World *world, std::vector<TileEntity *> *tileEntities, int_t posX, int_t posY, int_t posZ, int_t size, int_t glListId);
	~WorldRenderer();

	void setPosition(int_t x, int_t y, int_t z);
	void updateInFrustrum(ICamera *icamera);
	void updateRenderer();
	void markDirty();
	void callOcclusionQueryList();
	int_t getGLCallListForPass(int_t pass);
	// A dirty mark caused by a light value change. With
	// PLATFORM_COALESCE_MESH_REBUILDS an active build keeps going and is
	// rebuilt once more after it completes, instead of restarting on every
	// frame of a light propagation (a torch is several frames of them).
	void markDirtyFromLighting();
	// Set by RenderGlobal for a block change next to the player; the scheduler
	// runs these ahead of streaming work and to completion.
	bool urgentRebuild = false;

	bool skipAllRenderPasses();
	bool skipRenderPass(int_t pass);
	float distanceToEntitySquared(Entity *entity);
	void setDontDraw();
	void detachFromWorld();
	void cleanup();

	static int_t chunksUpdated;

	// Public fields matching Java
	World *worldObj;
	bool needsUpdate;
	bool queuedForUpdate = false;
	bool isChunkLit;
	bool isWaitingOnOcclusionQuery;
	bool isVisible;
	bool isInFrustum;
	// Stronger than isInFrustum: PS2 uses it for its clip fast path and desktop
	// Fancy Occlusion uses it to avoid querying boxes that cross a frustum plane.
	bool isFullyInFrustum;
	bool isVisibleFromPosition;
	double visibleFromX;
	double visibleFromY;
	double visibleFromZ;
	int_t glOcclusionQuery;
	int_t chunkIndex;

	int_t posX;
	int_t posY;
	int_t posZ;
	int_t sizeWidth;
	int_t sizeHeight;
	int_t sizeDepth;

	// Center positions (posX + size/2) for distance calc
	int_t posXPlus;
	int_t posYPlus;
	int_t posZPlus;

	// Clip positions (i & 0x3ff)
	int_t posXClip;
	int_t posYClip;
	int_t posZClip;

	// Offsets (i - posXClip) — used by RenderGlobal for translation
	int_t posXMinus;
	int_t posYMinus;
	int_t posZMinus;

	AxisAlignedBB *rendererBoundingBox;

	// Tile entities with special renderers collected during updateRenderer()
	std::vector<TileEntity *> tileEntityRenderers;

private:
	int_t glRenderList;
	bool needsOcclusionBoxUpdate;
	void updateOcclusionBox();
	bool isInitialized;
	bool _skipRenderPass[2];

	static void eraseAllTileEntityRefs(std::vector<TileEntity *> *list, TileEntity *te);
	static void pushUniqueTileEntityRef(std::vector<TileEntity *> *list, TileEntity *te);
	void removeTileEntityRenderersFromGlobalList();
	std::vector<TileEntity *> *tileEntities;
};