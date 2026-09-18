#include "pc/render/PcRenderBackendApi.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <cstring>
#include <utility>

#include "pc/render/d3d9/PcD3D9Context.h"
#include "pc/render/d3d9/PcD3D9Internal.h"

namespace
{
void copyRgbaToLockedRect(const void* pixels, int width, int height, const D3DLOCKED_RECT& locked)
{
    if (pixels == nullptr || width <= 0 || height <= 0)
        return;

    const unsigned char* source = static_cast<const unsigned char*>(pixels);
    for (int y = 0; y < height; ++y)
    {
        unsigned char* destination = static_cast<unsigned char*>(locked.pBits) +
                                     static_cast<std::size_t>(y) * static_cast<std::size_t>(locked.Pitch);
        const unsigned char* sourceRow = source + static_cast<std::size_t>(y) *
                                         static_cast<std::size_t>(width) * 4u;
        for (int x = 0; x < width; ++x)
        {
            const unsigned char* src = sourceRow + static_cast<std::size_t>(x) * 4u;
            unsigned char* dst = destination + static_cast<std::size_t>(x) * 4u;
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
        }
    }
}

D3DTEXTUREFILTERTYPE minFilter(bool blur, int mipmapLevel, bool mipmapLinear)
{
    if (mipmapLevel <= 0)
        return blur ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    if (blur)
        return D3DTEXF_LINEAR;
    return D3DTEXF_POINT;
}
}

IDirect3DTexture9* pcD3D9LookupTexture(int textureId)
{
    auto& textures = pcD3D9State().textures;
    const auto it = textures.find(textureId);
    return it == textures.end() ? nullptr : it->second.texture;
}

void pcD3D9BindTextureStage(int stage)
{
    if (stage < 0 || stage > 1)
        return;
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    auto& state = pcD3D9State();
    const std::size_t index = static_cast<std::size_t>(stage);
    IDirect3DTexture9* texture = state.textureEnabled[index]
        ? state.boundTextures[index]
        : nullptr;
    if (state.appliedTextureValid[index] && state.appliedTextures[index] == texture)
        return;

    device->SetTexture(stage, texture);
    state.appliedTextures[index] = texture;
    state.appliedTextureValid[index] = true;
}

void pcD3D9ClearTextures()
{
    auto& state = pcD3D9State();
    state.textures.clear();
    state.boundTextureIds = {{0, 0}};
    state.boundTextures = {{nullptr, nullptr}};
    state.appliedTextures = {{nullptr, nullptr}};
    state.appliedTextureValid = {{true, true}};
    pcD3D9MarkDrawStateDirty();
    if (IDirect3DDevice9* device = pcD3D9Device())
    {
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
    }
}

namespace PcD3D9RenderBackend
{
void renderBindTexture(int texture)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordBindTexture(texture);
        return;
    }
    auto& state = pcD3D9State();
    const std::size_t index = static_cast<std::size_t>(state.activeTextureUnit);
    if (state.boundTextureIds[index] == texture)
        return;
    const bool wasStageTextured = state.textureEnabled[index] && state.boundTextures[index] != nullptr;
    state.boundTextureIds[index] = texture;
    state.boundTextures[index] = pcD3D9LookupTexture(texture);
    const bool isStageTextured = state.textureEnabled[index] && state.boundTextures[index] != nullptr;
    if (wasStageTextured != isStageTextured)
        pcD3D9MarkDrawStateDirty();
    pcD3D9BindTextureStage(state.activeTextureUnit);
}

void renderSetActiveTextureUnit(int textureUnit)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordActiveTexture(textureUnit);
        return;
    }
    pcD3D9State().activeTextureUnit = pcD3D9TextureStageFromConstant(textureUnit);
}

void renderSetClientActiveTextureUnit(int textureUnit)
{
    pcD3D9State().clientTextureUnit = pcD3D9TextureStageFromConstant(textureUnit);
}

void renderSetMultiTextureCoord(int textureUnit, float u, float v)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordMultiTextureCoord(textureUnit, u, v);
        return;
    }
    const int stage = pcD3D9TextureStageFromConstant(textureUnit);
    auto& state = pcD3D9State();
    const std::size_t index = static_cast<std::size_t>(stage);
    if (state.currentTexCoord[index][0] == u && state.currentTexCoord[index][1] == v)
        return;
    state.currentTexCoord[index] = {{u, v}};
    pcD3D9MarkTextureTransformDirty(stage);
}

void renderSetLightmapColors(const std::uint32_t*, int)
{
}

void renderGenerateTextures(int count, int* textures)
{
    if (count <= 0 || textures == nullptr)
        return;
    auto& state = pcD3D9State();
    for (int i = 0; i < count; ++i)
        textures[i] = state.nextTextureId++;
}

void renderDeleteTextures(int count, const int* textures)
{
    if (count <= 0 || textures == nullptr)
        return;
    auto& state = pcD3D9State();
    for (int i = 0; i < count; ++i)
    {
        const int id = textures[i];
        for (int stage = 0; stage < 2; ++stage)
        {
            if (state.boundTextureIds[static_cast<std::size_t>(stage)] == id)
            {
                state.boundTextureIds[static_cast<std::size_t>(stage)] = 0;
                state.boundTextures[static_cast<std::size_t>(stage)] = nullptr;
                state.appliedTextures[static_cast<std::size_t>(stage)] = nullptr;
                state.appliedTextureValid[static_cast<std::size_t>(stage)] = true;
                pcD3D9MarkDrawStateDirty();
                if (IDirect3DDevice9* device = pcD3D9Device())
                    device->SetTexture(stage, nullptr);
            }
        }
        state.textures.erase(id);
    }
}

bool renderTextureBeginUpload(int texture, int width, int height, int maxLevel,
                              bool blur, bool clamp, bool, bool)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr || texture <= 0 || width <= 0 || height <= 0)
        return false;

    PcD3D9TextureRecord record;
    record.width = width;
    record.height = height;
    record.maxLevel = std::max(maxLevel, 0);
    record.blur = blur;
    record.clamp = clamp;
    const UINT levels = static_cast<UINT>(record.maxLevel + 1);
    if (FAILED(device->CreateTexture(static_cast<UINT>(width), static_cast<UINT>(height), levels, 0,
                                     D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &record.texture, nullptr)))
        return false;

    auto& state = pcD3D9State();
    state.textures[texture] = std::move(record);
    IDirect3DTexture9* uploadedTexture = pcD3D9LookupTexture(texture);
    for (int stage = 0; stage < 2; ++stage)
    {
        const std::size_t index = static_cast<std::size_t>(stage);
        if (state.boundTextureIds[index] == texture)
            state.boundTextures[index] = uploadedTexture;
    }
    const std::size_t activeIndex = static_cast<std::size_t>(state.activeTextureUnit);
    state.boundTextureIds[activeIndex] = texture;
    state.boundTextures[activeIndex] = uploadedTexture;
    pcD3D9MarkDrawStateDirty();
    pcD3D9BindTextureStage(state.activeTextureUnit);
    return true;
}

bool renderTextureIsValid(int texture)
{
    return texture > 0 && pcD3D9LookupTexture(texture) != nullptr;
}

void renderTextureImageRgba(int level, int width, int height, const void* pixels)
{
    if (level < 0 || width <= 0 || height <= 0 || pixels == nullptr)
        return;
    const auto& state = pcD3D9State();
    IDirect3DTexture9* texture = state.boundTextures[static_cast<std::size_t>(state.activeTextureUnit)];
    if (texture == nullptr || static_cast<UINT>(level) >= texture->GetLevelCount())
        return;

    D3DLOCKED_RECT locked{};
    if (FAILED(texture->LockRect(static_cast<UINT>(level), &locked, nullptr, 0)))
        return;
    copyRgbaToLockedRect(pixels, width, height, locked);
    texture->UnlockRect(static_cast<UINT>(level));
}

void renderTextureSubImageRgba(int level, int x, int y, int width, int height, const void* pixels)
{
    if (level < 0 || width <= 0 || height <= 0 || pixels == nullptr)
        return;
    const auto& state = pcD3D9State();
    IDirect3DTexture9* texture = state.boundTextures[static_cast<std::size_t>(state.activeTextureUnit)];
    if (texture == nullptr || static_cast<UINT>(level) >= texture->GetLevelCount())
        return;

    RECT region{x, y, x + width, y + height};
    D3DLOCKED_RECT locked{};
    if (FAILED(texture->LockRect(static_cast<UINT>(level), &locked, &region, 0)))
        return;
    copyRgbaToLockedRect(pixels, width, height, locked);
    texture->UnlockRect(static_cast<UINT>(level));
}

void renderTextureParameters(bool blur, bool mipmaps, bool clamp)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    const DWORD stage = static_cast<DWORD>(pcD3D9State().activeTextureUnit);
    pcD3D9SetSamplerState(stage, D3DSAMP_MINFILTER, blur ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    pcD3D9SetSamplerState(stage, D3DSAMP_MAGFILTER, blur ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    pcD3D9SetSamplerState(stage, D3DSAMP_MIPFILTER, mipmaps ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    pcD3D9SetSamplerState(stage, D3DSAMP_ADDRESSU, clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
    pcD3D9SetSamplerState(stage, D3DSAMP_ADDRESSV, clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
}

void renderApplyTextureQuality(bool blur, int mipmapLevel, bool mipmapLinear, int anisotropy)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    const DWORD stage = static_cast<DWORD>(pcD3D9State().activeTextureUnit);
    D3DCAPS9 caps{};
    device->GetDeviceCaps(&caps);
    const int requestedAnisotropy = std::max(anisotropy, 1);
    const DWORD maximumAnisotropy = std::max<DWORD>(caps.MaxAnisotropy, 1u);
    const DWORD usedAnisotropy = std::min<DWORD>(static_cast<DWORD>(requestedAnisotropy), maximumAnisotropy);
    const bool anisotropic = usedAnisotropy > 1 && (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;

    pcD3D9SetSamplerState(stage, D3DSAMP_MINFILTER,
                            anisotropic ? D3DTEXF_ANISOTROPIC : minFilter(blur, mipmapLevel, mipmapLinear));
    pcD3D9SetSamplerState(stage, D3DSAMP_MAGFILTER, blur ? D3DTEXF_LINEAR : D3DTEXF_POINT);
    pcD3D9SetSamplerState(stage, D3DSAMP_MIPFILTER,
                            mipmapLevel > 0 ? (mipmapLinear ? D3DTEXF_LINEAR : D3DTEXF_POINT) : D3DTEXF_NONE);
    pcD3D9SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, usedAnisotropy);
    pcD3D9SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, 0);
}

int renderGetMaxAnisotropy()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return 1;
    D3DCAPS9 caps{};
    if (FAILED(device->GetDeviceCaps(&caps)) || (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) == 0)
        return 1;
    return static_cast<int>(std::max<DWORD>(caps.MaxAnisotropy, 1u));
}

int renderGetMaxSamples()
{
    return pcD3D9GetSamples();
}

void renderResetResources()
{
    pcD3D9ClearDisplayLists();
    pcD3D9ClearQueries();
    pcD3D9ClearTextures();
}
}

#endif
