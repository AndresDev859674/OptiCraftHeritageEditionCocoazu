#pragma once

#include "platform/RenderAPI.h"

#include <cstring>
#include <limits>
#include <new>

// Retain unlit attributes. Terrain's compact capture deliberately bakes light
// and drops normals; model and inventory vertices need both at replay time.
inline bool ps2CaptureLitInterleaved(const RenderInterleavedMesh& mesh,
                                   RenderCapturedMesh& out, bool append)
{
    if (mesh.data == nullptr || mesh.stride != 32 || mesh.first < 0 || mesh.count <= 0 ||
        mesh.positionShort || !mesh.hasNormals || mesh.normalOffset != 24 ||
        (mesh.hasTexture && mesh.texCoordOffset != 12) ||
        (mesh.hasColor && mesh.colorOffset != 20) ||
        (mesh.hasBrightness && mesh.brightnessOffset != 28))
        return false;
    if (!append)
        out.clear();
    if (!out.empty() && (out.stride != 32 || out.primitive != mesh.primitive ||
        !out.hasNormals || out.normalOffset != 24 ||
        out.hasTexture != mesh.hasTexture || out.hasColor != mesh.hasColor ||
        out.hasBrightness != mesh.hasBrightness))
        return false;
    if (mesh.count > std::numeric_limits<int>::max() - out.vertexCount)
        return false;
    const std::size_t base = out.raw.size();
    const std::size_t count = static_cast<std::size_t>(mesh.count);
    if (count > (out.raw.max_size() - base) / 8)
        return false;
    try
    {
        out.raw.resize(base + count * 8);
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    const unsigned char* src = static_cast<const unsigned char*>(mesh.data) +
        static_cast<std::size_t>(mesh.first) * 32;
    std::int32_t* dst = out.raw.data() + base;
    for (int vertex = 0; vertex < mesh.count; ++vertex, src += 32, dst += 8)
    {
        std::memcpy(dst, src, 12);
        dst[3] = dst[4] = dst[5] = dst[6] = dst[7] = 0;
        if (mesh.hasTexture)
            std::memcpy(dst + 3, src + 12, 8);
        if (mesh.hasColor)
            std::memcpy(dst + 5, src + 20, 4);
        std::memcpy(dst + 6, src + 24, 3);
        if (mesh.hasBrightness)
            std::memcpy(dst + 7, src + 28, 4);
    }
    out.stride = 32;
    out.primitive = mesh.primitive;
    out.positionShort = false;
    out.hasTexture = mesh.hasTexture;
    out.texCoordOffset = 12;
    out.hasColor = mesh.hasColor;
    out.colorOffset = 20;
    out.hasNormals = true;
    out.normalOffset = 24;
    out.hasBrightness = mesh.hasBrightness;
    out.brightnessOffset = 28;
    out.vertexCount += mesh.count;
    return true;
}
