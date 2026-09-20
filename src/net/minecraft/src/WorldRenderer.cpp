#include "WorldRenderer.h"
#include "java/Arithmetic.h"

#include "platform/RenderAPI.h"
#include "platform/RenderTerrainAPI.h"
#include "World.h"
#include "Config.h"
#include "ConnectedTextures.h"
#include "ICamera.h"
#include "Entity.h"
#include "MathHelper.h"
#include "Block.h"
#include "RenderBlocks.h"
#include "RenderItem.h"
#include "Tessellator.h"
#include "Chunk.h"
#include "ChunkCache.h"
#include "TileEntity.h"
#include "TileEntityRenderer.h"
#include "AxisAlignedBB.h"

#include <algorithm>
#include <cstdint>
#include <utility>

// Not PS2-only: PLATFORM_RENDERER_AABB_MARGIN is used unconditionally further
// down, and this header is the platform-neutral place it comes from (it only
// pulls in ps2/render/Ps2Tuning.h when PLATFORM_PS2 is set). It reached the PC build
// transitively via ChunkProvider.h, which is luck rather than design.
#include "platform/PlatformTuning.h"
#include "platform/PlatformCompat.h"
#include "platform/ExtendedProfiler.h"

int_t WorldRenderer::chunksUpdated = 0;

namespace
{
	static float rendererAabbMargin()
	{
		// OptiFine C6 uses the exact 16x16x16 section box for both frustum and
		// occlusion tests. A zero margin also makes Fancy Occlusion's "fully in
		// frustum" classification useful instead of inflating every section.
		return 0.0f;
	}
}

void WorldRenderer::eraseAllTileEntityRefs(std::vector<TileEntity *> *list, TileEntity *te)
{
	if (list == nullptr || te == nullptr)
		return;
	list->erase(std::remove(list->begin(), list->end(), te), list->end());
}

void WorldRenderer::pushUniqueTileEntityRef(std::vector<TileEntity *> *list, TileEntity *te)
{
	if (list == nullptr || te == nullptr)
		return;
	if (std::find(list->begin(), list->end(), te) == list->end())
		list->push_back(te);
}

WorldRenderer::WorldRenderer(World *world, std::vector<TileEntity *> *tileEntitiesIn, int_t posX, int_t posY, int_t posZ, int_t size, int_t glListId)
{
	worldObj = world;
	tileEntities = tileEntitiesIn;
	sizeWidth = sizeHeight = sizeDepth = size;
	glRenderList = glListId;
	needsUpdate = false;
	isChunkLit = false;
	isWaitingOnOcclusionQuery = false;
	isVisible = true;
	isInFrustum = false;
	isFullyInFrustum = false;
	isVisibleFromPosition = false;
	visibleFromX = 0.0;
	visibleFromY = 0.0;
	visibleFromZ = 0.0;
	needsOcclusionBoxUpdate = false;
	glOcclusionQuery = 0;
	chunkIndex = 0;
	isInitialized = false;
	_skipRenderPass[0] = false;
	_skipRenderPass[1] = false;
	rendererBoundingBox = nullptr;

	this->posX = -999;
	setPosition(posX, posY, posZ);
	needsUpdate = false;
	queuedForUpdate = false;
}

WorldRenderer::~WorldRenderer()
{
	cleanup();
}

void WorldRenderer::removeTileEntityRenderersFromGlobalList()
{
	// RenderGlobal::tileEntities is a non-owning render list.  A WorldRenderer is
	// the owner of the references it contributed to that list.  When the renderer
	// is recycled, disabled or destroyed, those references must be removed before
	// the world/chunk code can invalidate and delete the TileEntity objects.
	for (TileEntity *te : tileEntityRenderers)
		eraseAllTileEntityRefs(tileEntities, te);
	tileEntityRenderers.clear();
}

void WorldRenderer::cleanup()
{
	// Display lists and occlusion queries come from ranges owned by
	// RenderGlobal. Deleting individual entries frees names that are reused by
	// new WorldRenderers and may then collide with model or sky display lists.
	setDontDraw();

	// rendererBoundingBox is created with AxisAlignedBB::getBoundingBox(), which
	// returns a heap allocation. Repositioning a renderer used to overwrite this
	// pointer and leak one AABB every time chunks were recycled around the player.
	delete rendererBoundingBox;
	rendererBoundingBox = nullptr;

	worldObj = nullptr;
	queuedForUpdate = false;

	glRenderList = 0;
	glOcclusionQuery = 0;
}

void WorldRenderer::setPosition(int_t x, int_t y, int_t z)
{
	if (x == posX && y == posY && z == posZ)
		return;

	setDontDraw();
	posX = x;
	posY = y;
	posZ = z;
	posXPlus = x + sizeWidth / 2;
	posYPlus = y + sizeHeight / 2;
	posZPlus = z + sizeDepth / 2;
	posXClip = x & 0x3ff;
	posYClip = y;
	posZClip = z & 0x3ff;
	posXMinus = x - posXClip;
	posYMinus = y - posYClip;
	posZMinus = z - posZClip;

	// Desktop C6 tests the exact section bounds so Fancy Occlusion can classify
	// fully-contained chunks accurately. Console backends keep their tuned
	// conservative margin because their fixed grids and clip paths are different.
	const float f = rendererAabbMargin();
	if (rendererBoundingBox == nullptr)
	{
		rendererBoundingBox = AxisAlignedBB::getBoundingBox(
			(float)x - f, (float)y - f, (float)z - f,
			(float)(x + sizeWidth) + f, (float)(y + sizeHeight) + f, (float)(z + sizeDepth) + f);
	}
	else
	{
		rendererBoundingBox->setBounds(
			(float)x - f, (float)y - f, (float)z - f,
			(float)(x + sizeWidth) + f, (float)(y + sizeHeight) + f, (float)(z + sizeDepth) + f);
	}

	// OptiFine C6 defers rebuilding the occlusion AABB list until this section
	// actually rebuilds. Repositioning a renderer grid can touch hundreds of
	// sections at once; compiling a list for every moved section here creates a
	// large synchronous spike before any useful terrain work begins.
	needsOcclusionBoxUpdate = true;
	isVisibleFromPosition = false;

	markDirty();
}

void WorldRenderer::updateOcclusionBox()
{
	if (!needsOcclusionBoxUpdate)
		return;

	const float f = rendererAabbMargin();
	renderBeginDisplayList(glRenderList + 2);
	RenderItem::renderAABB(AxisAlignedBB::getBoundingBoxFromPool(
		(float)posXClip - f, (float)posYClip - f, (float)posZClip - f,
		(float)(posXClip + sizeWidth) + f, (float)(posYClip + sizeHeight) + f, (float)(posZClip + sizeDepth) + f));
	renderEndDisplayList();
	needsOcclusionBoxUpdate = false;
}

void WorldRenderer::updateInFrustrum(ICamera *icamera)
{
	if (Config::isOcclusionFancy())
	{
		const int cls = icamera->classifyBoundingBox(rendererBoundingBox);
		isInFrustum = (cls != 0);
		isFullyInFrustum = (cls == 2);
	}
	else
	{
		isInFrustum = icamera->isBoundingBoxInFrustum(rendererBoundingBox);
		isFullyInFrustum = false;
	}
}

void WorldRenderer::updateRenderer()
{
	if (!needsUpdate)
		return;

	updateOcclusionBox();
	isVisibleFromPosition = false;

	// Vanilla ChunkCache synchronously requested every source chunk. This port's
	// shared ChunkCache deliberately treats missing chunks as air for pathfinding,
	// so the desktop renderer must restore that loading contract explicitly. A
	// provider that cannot supply the chunk yet (for example multiplayer) leaves
	// this renderer dirty and it will retry after the real column is published.
	{
		const int_t ccx0 = JavaArithmetic::intShr(posX - 1, 4);
		const int_t ccx1 = JavaArithmetic::intShr(posX + sizeWidth + 1, 4);
		const int_t ccz0 = JavaArithmetic::intShr(posZ - 1, 4);
		const int_t ccz1 = JavaArithmetic::intShr(posZ + sizeDepth + 1, 4);
		for (int_t ccx = ccx0; ccx <= ccx1; ++ccx)
		{
			for (int_t ccz = ccz0; ccz <= ccz1; ++ccz)
			{
				if (!worldObj->chunkExists(ccx, ccz))
					worldObj->getChunkFromChunkCoords(ccx, ccz);
				if (!worldObj->chunkExists(ccx, ccz))
					return;
			}
		}
	}

	chunksUpdated++;

	int_t x0 = posX, y0 = posY, z0 = posZ;
	int_t x1 = posX + sizeWidth, y1 = posY + sizeHeight, z1 = posZ + sizeDepth;

	std::vector<TessellatorTextureMesh> stagedExtraTextureMeshes[2];
	for (int_t k1 = 0; k1 < 2; k1++)
		_skipRenderPass[k1] = true;

	Chunk::isLit = false;

	// tileEntityRenderers still holds the old list here (reassigned at the end
	// of this function), so it doubles as the "old" side of the diff below —
	// no need to copy it into a set.
	std::vector<TileEntity *> rebuiltTileEntityRenderers;

	int_t margin = 1;
	ChunkCache chunkcache(worldObj, x0 - margin, y0 - margin, z0 - margin,
						  x1 + margin, y1 + margin, z1 + margin);

	RenderBlocks renderblocks(&chunkcache);

	Tessellator *tessellator = &Tessellator::instance;

	for (int_t pass = 0; pass < 2;)
	{
		bool hasOtherPass = false;
		bool drewAnything = false;
		bool listOpen = false;

		for (int_t y = y0; y < y1; y++)
		{
			for (int_t z = z0; z < z1; z++)
			{
				for (int_t x = x0; x < x1; x++)
				{
					int_t id = chunkcache.getBlockId(x, y, z);
					if (id <= 0)
						continue;

					if (!listOpen)
					{
						listOpen = true;
						renderBeginDisplayList(glRenderList + pass);
						renderPushMatrix();
						// Translate to the clip-space origin of this chunk
						renderTranslate((float)posXClip, (float)posYClip, (float)posZClip);
						// Slight scale to avoid z-fighting on chunk borders
						float f = 1.000001f;
						renderTranslate(-(float)sizeDepth / 2.0f, -(float)sizeHeight / 2.0f, -(float)sizeDepth / 2.0f);
						renderScale(f, f, f);
						renderTranslate((float)sizeDepth / 2.0f, (float)sizeHeight / 2.0f, (float)sizeDepth / 2.0f);
						tessellator->startDrawingQuads();
						tessellator->setTranslationD(-(double)posX, -(double)posY, -(double)posZ);
					}

					// Collect tile-entity special renderers on pass 0
					if (pass == 0 && Block::isBlockContainer[id])
					{
						TileEntity *te = chunkcache.getBlockTileEntity(x, y, z);
						if (te != nullptr && TileEntityRenderer::instance.hasSpecialRenderer(te) &&
							std::find(rebuiltTileEntityRenderers.begin(), rebuiltTileEntityRenderers.end(), te) == rebuiltTileEntityRenderers.end())
							rebuiltTileEntityRenderers.push_back(te);
					}

					Block *block = Block::blocksList[id];
					int_t blockPass = block->getRenderBlockPass();

					if (blockPass != pass)
					{
						hasOtherPass = true;
						continue;
					}

					drewAnything |= renderblocks.renderBlockByRenderType(block, x, y, z);
				}
			}
		}

		if (listOpen)
		{
			tessellator->captureTextureGroups(stagedExtraTextureMeshes[pass]);
			tessellator->draw();
			for (const TessellatorTextureMesh &group : stagedExtraTextureMeshes[pass])
			{
				renderBindTexture(group.textureId);
				(void)renderDrawCaptured(group.mesh);
			}
			if (!stagedExtraTextureMeshes[pass].empty())
				renderBindTexture(ConnectedTextures::getTerrainTextureId());
			renderPopMatrix();
			renderEndDisplayList();
			tessellator->setTranslationD(0.0, 0.0, 0.0);
		}
		else
		{
			drewAnything = false;
		}

		if (drewAnything || !stagedExtraTextureMeshes[pass].empty())
		{
			_skipRenderPass[pass] = false;
		}

		if (!hasOtherPass)
			break;

		pass++;
	}

	// Propagate tile-entity changes to the global list (linear scans — see the
	// PS2 build path earlier in this file for why hash sets aren't worth it).
	// Added: in the rebuilt list but not the old one
	for (TileEntity *te : rebuiltTileEntityRenderers)
	{
		if (std::find(tileEntityRenderers.begin(), tileEntityRenderers.end(), te) == tileEntityRenderers.end())
			pushUniqueTileEntityRef(tileEntities, te);
	}
	// Removed: in the old list but not the rebuilt one
	for (TileEntity *te : tileEntityRenderers)
	{
		if (std::find(rebuiltTileEntityRenderers.begin(), rebuiltTileEntityRenderers.end(), te) == rebuiltTileEntityRenderers.end())
		{
			eraseAllTileEntityRefs(tileEntities, te);
		}
	}

	isChunkLit = Chunk::isLit;
	isInitialized = true;
	tileEntityRenderers = rebuiltTileEntityRenderers;
	needsUpdate = false;
}

void WorldRenderer::markDirty()
{
	needsUpdate = true;
}

void WorldRenderer::markDirtyFromLighting()
{
	// The console/legacy builds coalesce an in-flight mesh rebuild here instead
	// of restarting it on every frame of a light propagation. The desktop path
	// has no incremental/staged terrain build to coalesce against, so a light
	// edit just marks the renderer dirty like any other change.
	markDirty();
}

void WorldRenderer::setDontDraw()
{
	// Whatever edit marked this renderer urgent was at its old position.
	urgentRebuild = false;
	removeTileEntityRenderersFromGlobalList();
	_skipRenderPass[0] = true;
	_skipRenderPass[1] = true;
	isInFrustum = false;
	isFullyInFrustum = false;
	isVisibleFromPosition = false;
	isInitialized = false;
}

void WorldRenderer::detachFromWorld()
{
	setDontDraw();
	worldObj = nullptr;
}

void WorldRenderer::callOcclusionQueryList()
{
	renderCallDisplayList(glRenderList + 2);
}

int_t WorldRenderer::getGLCallListForPass(int_t pass)
{
	if (!isInFrustum)
		return -1;
	if (!_skipRenderPass[pass])
		return glRenderList + pass;
	return -1;
}

bool WorldRenderer::skipAllRenderPasses()
{
	if (!isInitialized)
		return false;
	return _skipRenderPass[0] && _skipRenderPass[1];
}

bool WorldRenderer::skipRenderPass(int_t pass)
{
	if (pass < 0 || pass > 1)
		return true;
	return _skipRenderPass[pass];
}

// Narrow first, subtract second. The result only ever ranks renderers (the
// EntitySorter comparator, the mesh-budget sort) or meets a 256.0f threshold,
// and it is already returned as a float, so the double subtraction bought
// nothing -- while costing three libgcc calls per axis on a CPU with no double
// FPU, several hundred times per frame from inside a sort comparator.

float WorldRenderer::distanceToEntitySquared(Entity *entity)
{
	if (entity == nullptr)
		return 0.0f;
	float dx = (float)entity->posX - (float)posXPlus;
	float dy = (float)entity->posY - (float)posYPlus;
	float dz = (float)entity->posZ - (float)posZPlus;
	return dx * dx + dy * dy + dz * dz;
}