#include "ps2/render/Ps2TerrainMeshView.h"

#ifdef PS2_PLATFORM

#include <algorithm>
#include <cstdint>

#include "platform/PlatformTuning.h"
#include "ps2/render/Ps2CaptureLayout.h"

namespace
{
constexpr int_t kMaxTerrainBatchVertices = 3072;
}

int_t ps2_terrain_max_batch_vertices()
{
    return kMaxTerrainBatchVertices;
}

int_t ps2_terrain_bounded_batch_size(int_t remaining, int_t drawMode)
{
    int_t count = std::min(remaining, kMaxTerrainBatchVertices);
    if ((drawMode == PS2_NATIVE_PRIM_QUADS && (PLATFORM_TESSELLATOR_CONVERT_QUADS != 0)) ||
        drawMode == PS2_NATIVE_PRIM_TRIANGLES)
    {
        count -= count % 3;
    }
    else if (drawMode == PS2_NATIVE_PRIM_QUADS)
    {
        count -= count % 4;
    }
    return count;
}

Ps2NativeMeshView ps2_terrain_make_raw_mesh(const Ps2TerrainSectionView& section,
                                            const int_t* raw,
                                            int_t vertexCount)
{
    Ps2NativeMeshView mesh;
    mesh.packedTerrain = false;
    mesh.clampRuns = nullptr;
    mesh.clampRunCount = 0;
    mesh.vertices = raw;
    mesh.vertexStride = Ps2CaptureLayout::Stride;
    mesh.vertexSize = 3;
    mesh.texCoords = section.hasTexture
        ? reinterpret_cast<const float*>(raw) + 3
        : nullptr;
    mesh.texCoordStride = Ps2CaptureLayout::Stride;
    mesh.texCoordEnabled = section.hasTexture;
    mesh.colors = section.hasColor
        ? reinterpret_cast<const uint8_t*>(raw) + Ps2CaptureLayout::ColorOffset
        : nullptr;
    mesh.colorStride = Ps2CaptureLayout::Stride;
    mesh.colorSize = 4;
    mesh.colorEnabled = section.hasColor;
    mesh.colorFloat = false;
    mesh.normals = nullptr;
    mesh.normalStride = 0;
    mesh.normalFloat = false;
    mesh.hasNormals = section.hasNormals;
    mesh.drawMode = (section.drawMode == PS2_NATIVE_PRIM_QUADS && (PLATFORM_TESSELLATOR_CONVERT_QUADS != 0))
        ? PS2_NATIVE_PRIM_TRIANGLES
        : section.drawMode;
    mesh.first = 0;
    mesh.count = vertexCount;
    mesh.slices = nullptr;
    mesh.sliceCount = 0;
    return mesh;
}

Ps2NativeMeshView ps2_terrain_make_packed_mesh(const short* positions,
                                               const short* texCoords,
                                               const unsigned char* colors,
                                               int_t firstVertex,
                                               int_t vertexCount,
                                               const Ps2NativeClampRun* clampRuns,
                                               int clampRunCount,
                                               const Ps2NativeSlice* slices,
                                               int sliceCount)
{
    Ps2NativeMeshView mesh;
    mesh.packedTerrain = true;
    mesh.clampRuns = clampRuns;
    mesh.clampRunCount = clampRunCount;
    mesh.vertices = positions;
    mesh.vertexStride = 4 * (int)sizeof(short);
    mesh.vertexSize = 3;
    mesh.texCoords = texCoords;
    mesh.texCoordStride = 2 * (int)sizeof(short);
    mesh.texCoordEnabled = true;
    mesh.colors = colors;
    mesh.colorStride = 4;
    mesh.colorSize = 4;
    mesh.colorEnabled = true;
    mesh.colorFloat = false;
    mesh.normals = nullptr;
    mesh.normalStride = 0;
    mesh.normalFloat = false;
    mesh.hasNormals = false;
    mesh.drawMode = PS2_NATIVE_PRIM_QUADS;
    mesh.first = firstVertex;
    mesh.count = vertexCount;
    mesh.slices = slices;
    mesh.sliceCount = sliceCount;
    return mesh;
}

#endif
