#include "pc/render/d3d9/PcD3D9Internal.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "pc/render/d3d9/PcD3D9Context.h"

namespace
{
PcD3D9BackendState g_state;

struct PcD3D9ResetStateSnapshot
{
    bool valid = false;
    decltype(g_state.renderStateValues) renderStateValues{};
    decltype(g_state.renderStateValid) renderStateValid{};
    decltype(g_state.textureStageStateValues) textureStageStateValues{};
    decltype(g_state.textureStageStateValid) textureStageStateValid{};
    decltype(g_state.samplerStateValues) samplerStateValues{};
    decltype(g_state.samplerStateValid) samplerStateValid{};
};

PcD3D9ResetStateSnapshot g_resetState;

DWORD floatBits(float value)
{
    DWORD bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "D3D render state float must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool drawLayoutChanged(bool hasTexture, bool hasColor, bool hasBrightness, bool hasNormals)
{
    return !g_state.drawStateValid ||
           g_state.drawHasTexture != hasTexture ||
           g_state.drawHasColor != hasColor ||
           g_state.drawHasBrightness != hasBrightness ||
           g_state.drawHasNormals != hasNormals;
}

void rememberDrawLayout(bool hasTexture, bool hasColor, bool hasBrightness, bool hasNormals)
{
    g_state.drawHasTexture = hasTexture;
    g_state.drawHasColor = hasColor;
    g_state.drawHasBrightness = hasBrightness;
    g_state.drawHasNormals = hasNormals;
    g_state.drawStateValid = true;
}

void applyDrawState(bool hasTexture, bool hasColor, bool hasBrightness, bool hasNormals)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    pcD3D9BindTextureStage(0);
    pcD3D9BindTextureStage(1);

    const bool stateChanged = drawLayoutChanged(hasTexture, hasColor, hasBrightness, hasNormals);
    const bool useLighting = g_state.lightingEnabled && hasNormals;

    if (stateChanged)
    {
        const D3DCOLOR factor = pcD3D9Color(g_state.currentColor[0], g_state.currentColor[1],
                                            g_state.currentColor[2], g_state.currentColor[3]);
        pcD3D9SetRenderState(D3DRS_TEXTUREFACTOR, factor);
        pcD3D9SetRenderState(D3DRS_LIGHTING, useLighting ? TRUE : FALSE);

        if (useLighting)
        {
            D3DMATERIAL9 material{};
            const bool useCurrentColor = g_state.colorMaterialEnabled && !hasColor;
            const float red = useCurrentColor ? g_state.currentColor[0] : 1.0f;
            const float green = useCurrentColor ? g_state.currentColor[1] : 1.0f;
            const float blue = useCurrentColor ? g_state.currentColor[2] : 1.0f;
            const float alpha = useCurrentColor ? g_state.currentColor[3] : 1.0f;
            material.Diffuse = D3DCOLORVALUE{red, green, blue, alpha};
            material.Ambient = material.Diffuse;
            material.Specular = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
            material.Emissive = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
            material.Power = 0.0f;
            device->SetMaterial(&material);
            pcD3D9SetRenderState(D3DRS_COLORVERTEX, hasColor ? TRUE : FALSE);
            pcD3D9SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, hasColor ? D3DMCS_COLOR1 : D3DMCS_MATERIAL);
            pcD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, hasColor ? D3DMCS_COLOR1 : D3DMCS_MATERIAL);
        }

        const bool stage0Textured = g_state.textureEnabled[0] && g_state.boundTextures[0] != nullptr;
        if (stage0Textured)
        {
            pcD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            pcD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            pcD3D9SetTextureStageState(0, D3DTSS_COLORARG2,
                                         useLighting ? D3DTA_DIFFUSE : (hasColor ? D3DTA_DIFFUSE : D3DTA_TFACTOR));
            pcD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            pcD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            pcD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2,
                                         useLighting ? D3DTA_DIFFUSE : (hasColor ? D3DTA_DIFFUSE : D3DTA_TFACTOR));
        }
        else
        {
            device->SetTexture(0, nullptr);
            g_state.appliedTextures[0] = nullptr;
            g_state.appliedTextureValid[0] = true;
            pcD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            pcD3D9SetTextureStageState(0, D3DTSS_COLORARG1,
                                         useLighting ? D3DTA_DIFFUSE : (hasColor ? D3DTA_DIFFUSE : D3DTA_TFACTOR));
            pcD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            pcD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1,
                                         useLighting ? D3DTA_DIFFUSE : (hasColor ? D3DTA_DIFFUSE : D3DTA_TFACTOR));
        }
        pcD3D9SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);

        const bool stage1Textured = g_state.textureEnabled[1] && g_state.boundTextures[1] != nullptr;
        if (stage1Textured)
        {
            pcD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
            pcD3D9SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
            pcD3D9SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TEXTURE);
            pcD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            pcD3D9SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
            pcD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
        }
        else
        {
            device->SetTexture(1, nullptr);
            g_state.appliedTextures[1] = nullptr;
            g_state.appliedTextureValid[1] = true;
            pcD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            pcD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
            pcD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            g_state.textureTransformModeValid[1] = false;
        }

        rememberDrawLayout(hasTexture, hasColor, hasBrightness, hasNormals);
    }

    if (g_state.textureEnabled[0] && g_state.boundTextures[0] != nullptr)
        pcD3D9ApplyTextureTransform(0, hasTexture);
    if (g_state.textureEnabled[1] && g_state.boundTextures[1] != nullptr)
        pcD3D9ApplyTextureTransform(1, hasBrightness);
}
}

PcD3D9TextureRecord::~PcD3D9TextureRecord()
{
    if (texture != nullptr)
        texture->Release();
}

PcD3D9TextureRecord::PcD3D9TextureRecord(PcD3D9TextureRecord&& other) noexcept
{
    *this = std::move(other);
}

PcD3D9TextureRecord& PcD3D9TextureRecord::operator=(PcD3D9TextureRecord&& other) noexcept
{
    if (this == &other)
        return *this;
    if (texture != nullptr)
        texture->Release();
    texture = other.texture;
    width = other.width;
    height = other.height;
    maxLevel = other.maxLevel;
    blur = other.blur;
    clamp = other.clamp;
    other.texture = nullptr;
    return *this;
}

PcD3D9BackendState& pcD3D9State()
{
    return g_state;
}

IDirect3DDevice9* pcD3D9Device()
{
    return pcD3D9GetDevice();
}

int pcD3D9TextureStageFromConstant(int textureUnit)
{
    // GL_TEXTURE0_ARB and GL_TEXTURE1_ARB are 0x84C0/0x84C1. The engine keeps
    // those values backend-neutral so existing OpenGlHelper call sites remain unchanged.
    if (textureUnit == 0x84C1 || textureUnit == 1)
        return 1;
    return 0;
}

PcD3D9MatrixStack& pcD3D9CurrentMatrixStack()
{
    if (g_state.matrixMode == RenderMatrixMode::Projection)
        return g_state.projection;
    if (g_state.matrixMode == RenderMatrixMode::Texture)
        return g_state.textureMatrices[static_cast<std::size_t>(g_state.activeTextureUnit)];
    return g_state.modelView;
}

void pcD3D9MarkCurrentMatrixDirty()
{
    if (g_state.matrixMode == RenderMatrixMode::Projection)
    {
        g_state.projectionTransformDirty = true;
        return;
    }
    if (g_state.matrixMode == RenderMatrixMode::Texture)
    {
        pcD3D9MarkTextureTransformDirty(g_state.activeTextureUnit);
        return;
    }
    g_state.modelViewTransformDirty = true;
}

void pcD3D9MarkDrawStateDirty()
{
    g_state.drawStateValid = false;
}

void pcD3D9MarkTextureTransformDirty(int stage)
{
    if (stage < 0 || stage > 1)
        return;
    g_state.textureTransformDirty[static_cast<std::size_t>(stage)] = true;
}

void pcD3D9ApplyTransforms()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    if (g_state.modelViewTransformDirty)
    {
        const D3DMATRIX world = pcD3D9MatrixToD3D(g_state.modelView.top());
        device->SetTransform(D3DTS_WORLD, &world);
        g_state.modelViewTransformDirty = false;
    }

    if (g_state.projectionTransformDirty)
    {
        const D3DMATRIX projection = pcD3D9ProjectionToD3D(g_state.projection.top());
        device->SetTransform(D3DTS_PROJECTION, &projection);
        g_state.projectionTransformDirty = false;
    }
}

void pcD3D9ApplyTextureTransform(int stage, bool hasPerVertexCoordinates)
{
    if (stage < 0 || stage > 1)
        return;
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    const std::size_t index = static_cast<std::size_t>(stage);
    if (!g_state.textureTransformDirty[index] &&
        g_state.textureTransformModeValid[index] &&
        g_state.textureTransformUsesVertexCoords[index] == hasPerVertexCoordinates)
        return;

    PcD3D9Matrix matrix = g_state.textureMatrices[index].top();
    if (!hasPerVertexCoordinates)
    {
        const auto& coordinate = g_state.currentTexCoord[index];
        matrix = pcD3D9MatrixMultiply(matrix, pcD3D9MatrixTranslation(coordinate[0], coordinate[1], 0.0f));
    }
    const D3DMATRIX d3dMatrix = pcD3D9TextureMatrixToD3D2D(matrix);
    device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + stage), &d3dMatrix);
    pcD3D9SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    g_state.textureTransformDirty[index] = false;
    g_state.textureTransformModeValid[index] = true;
    g_state.textureTransformUsesVertexCoords[index] = hasPerVertexCoordinates;
}

void pcD3D9ApplyDrawState(const PcD3D9RetainedMesh& mesh)
{
    pcD3D9ApplyTransforms();
    applyDrawState(mesh.hasTexture, mesh.hasColor, mesh.hasBrightness, mesh.hasNormals);
}

void pcD3D9ApplyDrawState(const PcD3D9PreparedMesh& mesh)
{
    pcD3D9ApplyTransforms();
    applyDrawState(mesh.hasTexture, mesh.hasColor, mesh.hasBrightness, mesh.hasNormals);
}

bool pcD3D9SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;

    const int index = static_cast<int>(state);
    if (index >= 0 && index < static_cast<int>(g_state.renderStateValues.size()))
    {
        const std::size_t cacheIndex = static_cast<std::size_t>(index);
        if (g_state.renderStateValid[cacheIndex] && g_state.renderStateValues[cacheIndex] == value)
            return true;
        if (FAILED(device->SetRenderState(state, value)))
            return false;
        g_state.renderStateValues[cacheIndex] = value;
        g_state.renderStateValid[cacheIndex] = true;
        return true;
    }

    return SUCCEEDED(device->SetRenderState(state, value));
}

bool pcD3D9SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;

    const int stateIndex = static_cast<int>(state);
    if (stage < g_state.textureStageStateValues.size() &&
        stateIndex >= 0 && stateIndex < static_cast<int>(g_state.textureStageStateValues[stage].size()))
    {
        const std::size_t cacheIndex = static_cast<std::size_t>(stateIndex);
        if (g_state.textureStageStateValid[stage][cacheIndex] &&
            g_state.textureStageStateValues[stage][cacheIndex] == value)
            return true;
        if (FAILED(device->SetTextureStageState(stage, state, value)))
            return false;
        g_state.textureStageStateValues[stage][cacheIndex] = value;
        g_state.textureStageStateValid[stage][cacheIndex] = true;
        return true;
    }

    return SUCCEEDED(device->SetTextureStageState(stage, state, value));
}

bool pcD3D9SetSamplerState(DWORD stage, D3DSAMPLERSTATETYPE state, DWORD value)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;

    const int stateIndex = static_cast<int>(state);
    if (stage < g_state.samplerStateValues.size() &&
        stateIndex >= 0 && stateIndex < static_cast<int>(g_state.samplerStateValues[stage].size()))
    {
        const std::size_t cacheIndex = static_cast<std::size_t>(stateIndex);
        if (g_state.samplerStateValid[stage][cacheIndex] &&
            g_state.samplerStateValues[stage][cacheIndex] == value)
            return true;
        if (FAILED(device->SetSamplerState(stage, state, value)))
            return false;
        g_state.samplerStateValues[stage][cacheIndex] = value;
        g_state.samplerStateValid[stage][cacheIndex] = true;
        return true;
    }

    return SUCCEEDED(device->SetSamplerState(stage, state, value));
}

bool pcD3D9SetFvf(DWORD fvf)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;
    if (g_state.appliedFvfValid && g_state.appliedFvf == fvf)
        return true;
    if (FAILED(device->SetFVF(fvf)))
        return false;
    g_state.appliedFvf = fvf;
    g_state.appliedFvfValid = true;
    return true;
}

bool pcD3D9SetVertexStream(IDirect3DVertexBuffer9* vertexBuffer, UINT stride)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr || vertexBuffer == nullptr)
        return false;
    if (g_state.appliedVertexStreamValid &&
        g_state.appliedVertexBuffer == vertexBuffer &&
        g_state.appliedVertexStride == stride)
        return true;
    if (FAILED(device->SetStreamSource(0, vertexBuffer, 0, stride)))
        return false;
    g_state.appliedVertexBuffer = vertexBuffer;
    g_state.appliedVertexStride = stride;
    g_state.appliedVertexStreamValid = true;
    return true;
}

bool pcD3D9SetIndexBuffer(IDirect3DIndexBuffer9* indexBuffer)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr || indexBuffer == nullptr)
        return false;
    if (g_state.appliedIndexBufferValid && g_state.appliedIndexBuffer == indexBuffer)
        return true;
    if (FAILED(device->SetIndices(indexBuffer)))
        return false;
    g_state.appliedIndexBuffer = indexBuffer;
    g_state.appliedIndexBufferValid = true;
    return true;
}

void pcD3D9InvalidateVertexBindings()
{
    g_state.appliedFvfValid = false;
    g_state.appliedVertexBuffer = nullptr;
    g_state.appliedVertexStride = 0;
    g_state.appliedVertexStreamValid = false;
    g_state.appliedIndexBuffer = nullptr;
    g_state.appliedIndexBufferValid = false;
}

void pcD3D9InvalidateDeviceStateCache()
{
    g_state.modelViewTransformDirty = true;
    g_state.projectionTransformDirty = true;
    g_state.textureTransformDirty = {{true, true}};
    g_state.textureTransformModeValid = {{false, false}};
    g_state.drawStateValid = false;
    g_state.appliedTextures = {{nullptr, nullptr}};
    g_state.appliedTextureValid = {{false, false}};
    g_state.renderStateValid.fill(false);
    for (auto& valid : g_state.textureStageStateValid)
        valid.fill(false);
    for (auto& valid : g_state.samplerStateValid)
        valid.fill(false);
    pcD3D9InvalidateVertexBindings();
}

D3DCOLOR pcD3D9Color(float r, float g, float b, float a)
{
    auto toByte = [](float value) -> unsigned int
    {
        value = std::max(0.0f, std::min(value, 1.0f));
        return static_cast<unsigned int>(value * 255.0f + 0.5f);
    };
    return D3DCOLOR_ARGB(toByte(a), toByte(r), toByte(g), toByte(b));
}

D3DCMPFUNC pcD3D9Compare(RenderCompare compare)
{
    switch (compare)
    {
        case RenderCompare::Never: return D3DCMP_NEVER;
        case RenderCompare::Less: return D3DCMP_LESS;
        case RenderCompare::Equal: return D3DCMP_EQUAL;
        case RenderCompare::LessEqual: return D3DCMP_LESSEQUAL;
        case RenderCompare::Greater: return D3DCMP_GREATER;
        case RenderCompare::NotEqual: return D3DCMP_NOTEQUAL;
        case RenderCompare::GreaterEqual: return D3DCMP_GREATEREQUAL;
        case RenderCompare::Always: return D3DCMP_ALWAYS;
    }
    return D3DCMP_ALWAYS;
}

D3DBLEND pcD3D9Blend(RenderBlendFactor factor)
{
    switch (factor)
    {
        case RenderBlendFactor::Zero: return D3DBLEND_ZERO;
        case RenderBlendFactor::One: return D3DBLEND_ONE;
        case RenderBlendFactor::SrcColor: return D3DBLEND_SRCCOLOR;
        case RenderBlendFactor::OneMinusSrcColor: return D3DBLEND_INVSRCCOLOR;
        case RenderBlendFactor::SrcAlpha: return D3DBLEND_SRCALPHA;
        case RenderBlendFactor::OneMinusSrcAlpha: return D3DBLEND_INVSRCALPHA;
        case RenderBlendFactor::DstAlpha: return D3DBLEND_DESTALPHA;
        case RenderBlendFactor::OneMinusDstAlpha: return D3DBLEND_INVDESTALPHA;
        case RenderBlendFactor::DstColor: return D3DBLEND_DESTCOLOR;
        case RenderBlendFactor::OneMinusDstColor: return D3DBLEND_INVDESTCOLOR;
    }
    return D3DBLEND_ONE;
}


void pcD3D9ForceFixedFunctionPipeline()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    // The Legacy renderer intentionally uses the D3D9 fixed-function pipeline.
    // Explicitly clear programmable shaders after CreateDevice/Reset so old
    // hardware never depends on PS/VS 2.0 or 3.0 support.
    device->SetVertexShader(nullptr);
    device->SetPixelShader(nullptr);
}

void pcD3D9CaptureStateForReset()
{
    g_resetState.renderStateValues = g_state.renderStateValues;
    g_resetState.renderStateValid = g_state.renderStateValid;
    g_resetState.textureStageStateValues = g_state.textureStageStateValues;
    g_resetState.textureStageStateValid = g_state.textureStageStateValid;
    g_resetState.samplerStateValues = g_state.samplerStateValues;
    g_resetState.samplerStateValid = g_state.samplerStateValid;
    g_resetState.valid = true;
}

void pcD3D9RestoreStateAfterReset()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    if (!g_resetState.valid)
    {
        pcD3D9RestoreDefaultState();
        return;
    }

    pcD3D9ForceFixedFunctionPipeline();
    pcD3D9InvalidateDeviceStateCache();

    for (std::size_t index = 0; index < g_resetState.renderStateValid.size(); ++index)
    {
        if (!g_resetState.renderStateValid[index])
            continue;
        pcD3D9SetRenderState(static_cast<D3DRENDERSTATETYPE>(index),
                             g_resetState.renderStateValues[index]);
    }

    for (std::size_t stage = 0; stage < g_resetState.textureStageStateValid.size(); ++stage)
    {
        for (std::size_t index = 0; index < g_resetState.textureStageStateValid[stage].size(); ++index)
        {
            if (!g_resetState.textureStageStateValid[stage][index])
                continue;
            pcD3D9SetTextureStageState(static_cast<DWORD>(stage),
                                       static_cast<D3DTEXTURESTAGESTATETYPE>(index),
                                       g_resetState.textureStageStateValues[stage][index]);
        }
    }

    for (std::size_t stage = 0; stage < g_resetState.samplerStateValid.size(); ++stage)
    {
        for (std::size_t index = 0; index < g_resetState.samplerStateValid[stage].size(); ++index)
        {
            if (!g_resetState.samplerStateValid[stage][index])
                continue;
            pcD3D9SetSamplerState(static_cast<DWORD>(stage),
                                  static_cast<D3DSAMPLERSTATETYPE>(index),
                                  g_resetState.samplerStateValues[stage][index]);
        }
    }

    for (std::size_t index = 0; index < g_state.lights.size(); ++index)
    {
        if (g_state.lightDefined[index])
            device->SetLight(static_cast<DWORD>(index), &g_state.lights[index]);
        device->LightEnable(static_cast<DWORD>(index), g_state.lightEnabled[index] ? TRUE : FALSE);
    }

    const D3DMATRIX view = pcD3D9MatrixToD3D(pcD3D9MatrixIdentity());
    device->SetTransform(D3DTS_VIEW, &view);
    pcD3D9ApplyTransforms();
    pcD3D9BindTextureStage(0);
    pcD3D9BindTextureStage(1);
    pcD3D9MarkDrawStateDirty();

    g_resetState.valid = false;
}

void pcD3D9RestoreDefaultState()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    g_resetState.valid = false;
    g_state.lightDefined.fill(false);
    g_state.lightEnabled.fill(false);
    pcD3D9ForceFixedFunctionPipeline();
    pcD3D9InvalidateDeviceStateCache();

    pcD3D9SetRenderState(D3DRS_ZENABLE, FALSE);
    pcD3D9SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    pcD3D9SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    pcD3D9SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    pcD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    pcD3D9SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    pcD3D9SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    pcD3D9SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    pcD3D9SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    pcD3D9SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
    pcD3D9SetRenderState(D3DRS_ALPHAREF, 0);
    pcD3D9SetRenderState(D3DRS_FOGENABLE, FALSE);
    pcD3D9SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    pcD3D9SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    pcD3D9SetRenderState(D3DRS_COLORWRITEENABLE,
                           D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                           D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    pcD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
    pcD3D9SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    pcD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
    pcD3D9SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
    pcD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    pcD3D9SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    pcD3D9SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    pcD3D9SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, floatBits(0.0f));
    pcD3D9SetRenderState(D3DRS_DEPTHBIAS, floatBits(0.0f));
    pcD3D9SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    pcD3D9SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    pcD3D9SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    pcD3D9SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    const D3DMATRIX view = pcD3D9MatrixToD3D(pcD3D9MatrixIdentity());
    device->SetTransform(D3DTS_VIEW, &view);
    pcD3D9ApplyTransforms();
    pcD3D9BindTextureStage(0);
    pcD3D9BindTextureStage(1);
}

#endif
