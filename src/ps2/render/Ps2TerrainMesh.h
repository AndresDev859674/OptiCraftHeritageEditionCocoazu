#pragma once

#ifdef PS2_PLATFORM

#include <cstddef>
#include <vector>

#include "java/Type.h"
#include "ps2/render/Ps2NativeDraw.h"

typedef Ps2NativeClampRun Ps2TerrainTileRun;

enum class Ps2TerrainMeshBuildFailure
{
    None,
    InvalidInput,
    Allocation,
    EmptyRuns
};

enum class Ps2TerrainMeshBuildStepResult
{
    Failed,
    Pending,
    Complete
};

struct Ps2TerrainMeshBuildStats
{
    unsigned int successful = 0;
    unsigned int invalidInput = 0;
    unsigned int allocationFailed = 0;
    unsigned int emptyRuns = 0;
};

Ps2TerrainMeshBuildStats ps2TerrainMeshBuildStats();

// Canonical immutable opaque chunk stream prepared once when a WorldRenderer
// mesh is published. VU1 consumes the aligned SoA streams through DMA REF;
// VU0 reads the same fixed-point data when clipping is required.
class Ps2TerrainMesh
{
public:
    Ps2TerrainMesh();

    bool build(const int_t* raw, std::size_t rawIntCount, int_t vertexCount,
               int_t drawMode, bool hasTexture, bool hasColor, bool hasNormals);
    Ps2TerrainMeshBuildStepResult beginBuild(const int_t* raw, std::size_t rawIntCount,
                                             int_t vertexCount, int_t drawMode,
                                             bool hasTexture, bool hasColor, bool hasNormals,
                                             bool& didWork);
    Ps2TerrainMeshBuildStepResult continueBuild(bool& didWork);
    void cancelBuild();
    bool buildInProgress() const { return m_buildActive; }
    void swap(Ps2TerrainMesh& other);
    void clearKeepCapacity();
    void release();

    bool valid() const { return m_valid; }
    int_t vertexCount() const { return m_vertexCount; }
    std::size_t ramBytes() const { return m_storage.capacity() + m_runs.capacity() * sizeof(Ps2TerrainTileRun); }
    const short* positions() const;
    const short* texCoords() const;
    const unsigned char* colors() const;
    const std::vector<Ps2TerrainTileRun>& runs() const { return m_runs; }

private:
    Ps2TerrainMeshBuildStepResult startNextBatch(bool& didWork);
    Ps2TerrainMeshBuildStepResult finishBuild();
    Ps2TerrainMeshBuildStepResult packRemainingCpu(bool& didWork);
    void resetBuildState();

    std::vector<unsigned char> m_storage;
    std::vector<Ps2TerrainTileRun> m_runs;
    std::size_t m_positionOffset;
    std::size_t m_texCoordOffset;
    std::size_t m_colorOffset;
    int_t m_vertexCount;
    bool m_valid;

    const int_t* m_buildRaw;
    int_t m_buildVertexCount;
    int_t m_buildFirstVertex;
    int_t m_buildBatchVertices;
    int_t m_buildBatchCapacity;
    int m_buildCurrentTileX;
    int m_buildCurrentTileY;
    bool m_buildHasColor;
    bool m_buildActive;
    bool m_buildVu0InFlight;
    bool m_buildBatchColorsProcessed;
};

#endif // PS2_PLATFORM
