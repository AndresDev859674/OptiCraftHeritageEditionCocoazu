#include "pc/render/PcRenderBackendApi.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <algorithm>
#include <cmath>
#include <cstring>

#include <SDL.h>

#include "pc/render/d3d9/PcD3D9Context.h"
#include "pc/render/d3d9/PcD3D9Internal.h"
#include "platform/Log.h"

namespace
{
DWORD floatBits(float value)
{
    DWORD bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

D3DCULL cullMode(RenderFace face)
{
    // RenderAPI follows OpenGL's default counter-clockwise front-face rule.
    // D3DCULL_* names the winding to reject, so culling GL back faces means
    // rejecting clockwise triangles and culling GL front faces means rejecting
    // counter-clockwise triangles.
    if (face == RenderFace::Front)
        return D3DCULL_CCW;
    if (face == RenderFace::Back)
        return D3DCULL_CW;
    return D3DCULL_NONE;
}

D3DFOGMODE fogMode(RenderFogMode mode)
{
    switch (mode)
    {
        case RenderFogMode::Exp: return D3DFOG_EXP;
        case RenderFogMode::Exp2: return D3DFOG_EXP2;
        case RenderFogMode::Linear:
        case RenderFogMode::EyeRadial:
        default: return D3DFOG_LINEAR;
    }
}

PcD3D9Matrix appended(const PcD3D9Matrix& current, const PcD3D9Matrix& operation)
{
    // OpenGL's fixed-function matrix calls post-multiply the current matrix.
    return pcD3D9MatrixMultiply(current, operation);
}

void setLightComponent(D3DLIGHT9& light, RenderLightParameter parameter, const float* values)
{
    if (values == nullptr)
        return;
    D3DCOLORVALUE color{values[0], values[1], values[2], values[3]};
    switch (parameter)
    {
        case RenderLightParameter::Ambient: light.Ambient = color; break;
        case RenderLightParameter::Diffuse: light.Diffuse = color; break;
        case RenderLightParameter::Specular: light.Specular = color; break;
        case RenderLightParameter::Position:
            light.Type = D3DLIGHT_DIRECTIONAL;
            // GL stores a directional light as the vector pointing toward the
            // source. D3D stores the direction the light rays travel.
            light.Direction.x = -values[0];
            light.Direction.y = -values[1];
            light.Direction.z = -values[2];
            break;
    }
}

D3DLIGHT9 readLight(IDirect3DDevice9* device, int index)
{
    auto& state = pcD3D9State();
    if (index >= 0 && index < static_cast<int>(state.lights.size()) &&
        state.lightDefined[static_cast<std::size_t>(index)])
        return state.lights[static_cast<std::size_t>(index)];

    D3DLIGHT9 light{};
    if (device == nullptr || FAILED(device->GetLight(static_cast<DWORD>(index), &light)))
    {
        light.Type = D3DLIGHT_DIRECTIONAL;
        light.Direction = D3DVECTOR{0.0f, -1.0f, 0.0f};
        light.Diffuse = D3DCOLORVALUE{1.0f, 1.0f, 1.0f, 1.0f};
        light.Ambient = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
        light.Specular = D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 1.0f};
    }
    return light;
}
}

namespace PcD3D9RenderBackend
{
void renderEnable(RenderCapability capability)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    auto& state = pcD3D9State();
    switch (capability)
    {
        case RenderCapability::Texture2D:
            state.textureEnabled[static_cast<std::size_t>(state.activeTextureUnit)] = true;
            pcD3D9MarkDrawStateDirty();
            pcD3D9BindTextureStage(state.activeTextureUnit);
            break;
        case RenderCapability::ColorMaterial:
            state.colorMaterialEnabled = true;
            pcD3D9MarkDrawStateDirty();
            pcD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
            break;
        case RenderCapability::CullFace:
            state.cullEnabled = true;
            state.cullAll = state.cullFace == RenderFace::FrontAndBack;
            pcD3D9SetRenderState(D3DRS_CULLMODE, cullMode(state.cullFace));
            break;
        case RenderCapability::AlphaTest: pcD3D9SetRenderState(D3DRS_ALPHATESTENABLE, TRUE); break;
        case RenderCapability::Blend: pcD3D9SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE); break;
        case RenderCapability::DepthTest: pcD3D9SetRenderState(D3DRS_ZENABLE, TRUE); break;
        case RenderCapability::Fog: pcD3D9SetRenderState(D3DRS_FOGENABLE, TRUE); break;
        case RenderCapability::Lighting:
            state.lightingEnabled = true;
            pcD3D9MarkDrawStateDirty();
            pcD3D9SetRenderState(D3DRS_LIGHTING, TRUE);
            break;
        case RenderCapability::Normalize:
        case RenderCapability::RescaleNormal: pcD3D9SetRenderState(D3DRS_NORMALIZENORMALS, TRUE); break;
        case RenderCapability::Light0:
            state.lightEnabled[0] = true;
            device->LightEnable(0, TRUE);
            break;
        case RenderCapability::Light1:
            state.lightEnabled[1] = true;
            device->LightEnable(1, TRUE);
            break;
        case RenderCapability::PolygonOffsetFill:
            state.polygonOffsetEnabled = true;
            pcD3D9SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, floatBits(state.polygonOffsetFactor));
            pcD3D9SetRenderState(D3DRS_DEPTHBIAS, floatBits(state.polygonOffsetUnits * 0.000001f));
            break;
    }
}

void renderDisable(RenderCapability capability)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    auto& state = pcD3D9State();
    switch (capability)
    {
        case RenderCapability::Texture2D:
            state.textureEnabled[static_cast<std::size_t>(state.activeTextureUnit)] = false;
            state.appliedTextures[static_cast<std::size_t>(state.activeTextureUnit)] = nullptr;
            state.appliedTextureValid[static_cast<std::size_t>(state.activeTextureUnit)] = true;
            pcD3D9MarkDrawStateDirty();
            device->SetTexture(state.activeTextureUnit, nullptr);
            break;
        case RenderCapability::ColorMaterial:
            state.colorMaterialEnabled = false;
            pcD3D9MarkDrawStateDirty();
            pcD3D9SetRenderState(D3DRS_COLORVERTEX, FALSE);
            break;
        case RenderCapability::CullFace:
            state.cullEnabled = false;
            state.cullAll = false;
            pcD3D9SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            break;
        case RenderCapability::AlphaTest: pcD3D9SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); break;
        case RenderCapability::Blend: pcD3D9SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); break;
        case RenderCapability::DepthTest: pcD3D9SetRenderState(D3DRS_ZENABLE, FALSE); break;
        case RenderCapability::Fog: pcD3D9SetRenderState(D3DRS_FOGENABLE, FALSE); break;
        case RenderCapability::Lighting:
            state.lightingEnabled = false;
            pcD3D9MarkDrawStateDirty();
            pcD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
            break;
        case RenderCapability::Normalize:
        case RenderCapability::RescaleNormal: pcD3D9SetRenderState(D3DRS_NORMALIZENORMALS, FALSE); break;
        case RenderCapability::Light0:
            state.lightEnabled[0] = false;
            device->LightEnable(0, FALSE);
            break;
        case RenderCapability::Light1:
            state.lightEnabled[1] = false;
            device->LightEnable(1, FALSE);
            break;
        case RenderCapability::PolygonOffsetFill:
            state.polygonOffsetEnabled = false;
            pcD3D9SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, floatBits(0.0f));
            pcD3D9SetRenderState(D3DRS_DEPTHBIAS, floatBits(0.0f));
            break;
    }
}

void renderBlendFunc(RenderBlendFactor source, RenderBlendFactor destination)
{
    if (IDirect3DDevice9* device = pcD3D9Device())
    {
        pcD3D9SetRenderState(D3DRS_SRCBLEND, pcD3D9Blend(source));
        pcD3D9SetRenderState(D3DRS_DESTBLEND, pcD3D9Blend(destination));
    }
}

void renderDepthMask(bool enabled)
{
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_ZWRITEENABLE, enabled ? TRUE : FALSE);
}

void renderDepthFunc(RenderCompare function)
{
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_ZFUNC, pcD3D9Compare(function));
}

void renderAlphaFunc(RenderCompare function, float reference)
{
    if (IDirect3DDevice9* device = pcD3D9Device())
    {
        pcD3D9SetRenderState(D3DRS_ALPHAFUNC, pcD3D9Compare(function));
        reference = std::max(0.0f, std::min(reference, 1.0f));
        pcD3D9SetRenderState(D3DRS_ALPHAREF, static_cast<DWORD>(reference * 255.0f + 0.5f));
    }
}

void renderCullFace(RenderFace face)
{
    auto& state = pcD3D9State();
    state.cullFace = face;
    state.cullAll = state.cullEnabled && face == RenderFace::FrontAndBack;
    if (state.cullEnabled)
    {
        if (IDirect3DDevice9* device = pcD3D9Device())
            pcD3D9SetRenderState(D3DRS_CULLMODE, cullMode(face));
    }
}

void renderColorMask(bool red, bool green, bool blue, bool alpha)
{
    DWORD mask = 0;
    if (red) mask |= D3DCOLORWRITEENABLE_RED;
    if (green) mask |= D3DCOLORWRITEENABLE_GREEN;
    if (blue) mask |= D3DCOLORWRITEENABLE_BLUE;
    if (alpha) mask |= D3DCOLORWRITEENABLE_ALPHA;
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_COLORWRITEENABLE, mask);
}

void renderColor4f(float r, float g, float b, float a)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordColor(r, g, b, a);
        return;
    }
    auto& state = pcD3D9State();
    if (state.currentColor[0] == r && state.currentColor[1] == g &&
        state.currentColor[2] == b && state.currentColor[3] == a)
        return;
    state.currentColor = {{r, g, b, a}};
    pcD3D9MarkDrawStateDirty();
}

void renderColor3f(float r, float g, float b)
{
    renderColor4f(r, g, b, 1.0f);
}

void renderNormal3f(float x, float y, float z)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordNormal(x, y, z);
        return;
    }
    pcD3D9State().currentNormal = {{x, y, z}};
}

void renderFogf(RenderFogParameter parameter, float value)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    switch (parameter)
    {
        case RenderFogParameter::Density: pcD3D9SetRenderState(D3DRS_FOGDENSITY, floatBits(value)); break;
        case RenderFogParameter::Start: pcD3D9SetRenderState(D3DRS_FOGSTART, floatBits(value)); break;
        case RenderFogParameter::End: pcD3D9SetRenderState(D3DRS_FOGEND, floatBits(value)); break;
        case RenderFogParameter::Mode:
        case RenderFogParameter::Color:
        case RenderFogParameter::DistanceMode: break;
    }
}

void renderFogi(RenderFogParameter parameter, RenderFogMode value)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    if (parameter == RenderFogParameter::DistanceMode)
    {
        pcD3D9SetRenderState(D3DRS_RANGEFOGENABLE, value == RenderFogMode::EyeRadial ? TRUE : FALSE);
        return;
    }
    if (parameter == RenderFogParameter::Mode)
    {
        // Vertex fog keeps compatibility with fixed-function transformed vertices
        // and works on Intel GMA-class D3D9 hardware without pixel shaders.
        pcD3D9SetRenderState(D3DRS_FOGVERTEXMODE, fogMode(value));
        pcD3D9SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    }
}

void renderFogColor(const float* values)
{
    if (values == nullptr)
        return;
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_FOGCOLOR, pcD3D9Color(values[0], values[1], values[2], values[3]));
}

void renderLightfv(int lightIndex, RenderLightParameter parameter, const float* values)
{
    if (lightIndex < 0 || lightIndex > 7 || values == nullptr)
        return;
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    D3DLIGHT9 light = readLight(device, lightIndex);
    if (parameter == RenderLightParameter::Position && values[3] == 0.0f)
    {
        const auto& matrix = pcD3D9State().modelView.top().values;
        float transformed[4] = {
            matrix[0] * values[0] + matrix[4] * values[1] + matrix[8] * values[2],
            matrix[1] * values[0] + matrix[5] * values[1] + matrix[9] * values[2],
            matrix[2] * values[0] + matrix[6] * values[1] + matrix[10] * values[2],
            0.0f
        };
        const float length = std::sqrt(transformed[0] * transformed[0] +
                                       transformed[1] * transformed[1] +
                                       transformed[2] * transformed[2]);
        if (length > 0.000001f)
        {
            transformed[0] /= length;
            transformed[1] /= length;
            transformed[2] /= length;
        }
        setLightComponent(light, parameter, transformed);
    }
    else
    {
        setLightComponent(light, parameter, values);
    }
    device->SetLight(static_cast<DWORD>(lightIndex), &light);
    auto& state = pcD3D9State();
    const std::size_t index = static_cast<std::size_t>(lightIndex);
    if (index < state.lights.size())
    {
        state.lights[index] = light;
        state.lightDefined[index] = true;
    }
}

void renderLightModelAmbient(const float* values)
{
    if (values == nullptr)
        return;
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_AMBIENT, pcD3D9Color(values[0], values[1], values[2], values[3]));
}

void renderColorMaterial(RenderFace, RenderColorMaterialMode mode)
{
    pcD3D9MarkDrawStateDirty();
    if (IDirect3DDevice9* device = pcD3D9Device())
    {
        pcD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
        pcD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,
                               mode == RenderColorMaterialMode::Ambient ? D3DMCS_COLOR1 : D3DMCS_COLOR1);
        if (mode == RenderColorMaterialMode::AmbientAndDiffuse)
            pcD3D9SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    }
}

void renderShadeModel(RenderShadeModel model)
{
    if (IDirect3DDevice9* device = pcD3D9Device())
        pcD3D9SetRenderState(D3DRS_SHADEMODE, model == RenderShadeModel::Smooth ? D3DSHADE_GOURAUD : D3DSHADE_FLAT);
}

void renderClear(unsigned int mask)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    DWORD flags = 0;
    if ((mask & RenderClearMask::Color) != 0) flags |= D3DCLEAR_TARGET;
    if ((mask & RenderClearMask::Depth) != 0) flags |= D3DCLEAR_ZBUFFER;
    if (flags == 0)
        return;

    D3DVIEWPORT9 previousViewport{};
    const bool havePreviousViewport = SUCCEEDED(device->GetViewport(&previousViewport));
    int backBufferWidth = 0;
    int backBufferHeight = 0;
    const bool haveBackBuffer = pcD3D9GetBackBufferSize(&backBufferWidth, &backBufferHeight);
    bool restoreViewport = false;
    if (havePreviousViewport && haveBackBuffer &&
        (previousViewport.X != 0 || previousViewport.Y != 0 ||
         previousViewport.Width != static_cast<DWORD>(backBufferWidth) ||
         previousViewport.Height != static_cast<DWORD>(backBufferHeight)))
    {
        D3DVIEWPORT9 fullViewport{};
        fullViewport.X = 0;
        fullViewport.Y = 0;
        fullViewport.Width = static_cast<DWORD>(backBufferWidth);
        fullViewport.Height = static_cast<DWORD>(backBufferHeight);
        fullViewport.MinZ = 0.0f;
        fullViewport.MaxZ = 1.0f;
        if (SUCCEEDED(device->SetViewport(&fullViewport)))
            restoreViewport = true;
        else
            MC_LOG_WARN("d3d9", "Failed to select full viewport for clear (%dx%d)\n", backBufferWidth, backBufferHeight);
    }

    const auto& state = pcD3D9State();
    const HRESULT clearResult = device->Clear(0, nullptr, flags,
        pcD3D9Color(state.clearColor[0], state.clearColor[1], state.clearColor[2], state.clearColor[3]),
        static_cast<float>(state.clearDepth), 0);
    if (FAILED(clearResult))
        MC_LOG_WARN("d3d9", "Clear failed (hr=0x%08lx)\n", static_cast<unsigned long>(clearResult));

    if (restoreViewport && FAILED(device->SetViewport(&previousViewport)))
        MC_LOG_WARN("d3d9", "Failed to restore viewport after clear\n");
}

void renderFinishGpu()
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;
    IDirect3DQuery9* query = nullptr;
    if (SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT, &query)) && query != nullptr)
    {
        query->Issue(D3DISSUE_END);
        while (query->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE)
            SDL_Delay(0);
        query->Release();
    }
}

void renderSubmitFrame()
{
}

void renderClearColor(float r, float g, float b, float a)
{
    pcD3D9State().clearColor = {{r, g, b, a}};
}

void renderClearDepth(double depth)
{
    pcD3D9State().clearDepth = std::max(0.0, std::min(depth, 1.0));
}

void renderPolygonOffset(float factor, float units)
{
    auto& state = pcD3D9State();
    state.polygonOffsetFactor = factor;
    state.polygonOffsetUnits = units;
    if (!state.polygonOffsetEnabled)
        return;
    if (IDirect3DDevice9* device = pcD3D9Device())
    {
        pcD3D9SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, floatBits(factor));
        pcD3D9SetRenderState(D3DRS_DEPTHBIAS, floatBits(units * 0.000001f));
    }
}

void renderLineWidth(float width)
{
    // D3D9 fixed-function line primitives are one pixel wide. Keep the value so
    // callers can query/debug state without introducing a shader-only line path.
    pcD3D9State().lineWidth = width;
}

void renderViewport(int x, int y, int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return;

    int backBufferWidth = 0;
    int backBufferHeight = 0;
    if (!pcD3D9GetBackBufferSize(&backBufferWidth, &backBufferHeight))
        return;

    const int d3dY = backBufferHeight - y - height;
    if (x < 0 || d3dY < 0 || x + width > backBufferWidth || d3dY + height > backBufferHeight)
    {
        MC_LOG_WARN("d3d9",
                    "Viewport outside backbuffer: gl=(%d,%d %dx%d) backbuffer=%dx%d\n",
                    x, y, width, height, backBufferWidth, backBufferHeight);
        return;
    }

    D3DVIEWPORT9 viewport{};
    viewport.X = static_cast<DWORD>(x);
    viewport.Y = static_cast<DWORD>(backBufferHeight - y - height);
    viewport.Width = static_cast<DWORD>(width);
    viewport.Height = static_cast<DWORD>(height);
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    if (SUCCEEDED(device->SetViewport(&viewport)))
    {
        pcD3D9State().viewport = {{x, y, width, height}};
        return;
    }

    MC_LOG_WARN("d3d9",
                "SetViewport failed: gl=(%d,%d %dx%d) d3d=(%lu,%lu %lux%lu)\n",
                x, y, width, height,
                static_cast<unsigned long>(viewport.X), static_cast<unsigned long>(viewport.Y),
                static_cast<unsigned long>(viewport.Width), static_cast<unsigned long>(viewport.Height));
}

void renderGetViewport(int* values)
{
    if (values == nullptr)
        return;
    const auto& viewport = pcD3D9State().viewport;
    for (int i = 0; i < 4; ++i)
        values[i] = viewport[static_cast<std::size_t>(i)];
}

void renderGetMatrix(RenderMatrixQuery query, float* values)
{
    if (values == nullptr)
        return;
    const PcD3D9Matrix* matrix = &pcD3D9State().modelView.top();
    if (query == RenderMatrixQuery::Projection)
        matrix = &pcD3D9State().projection.top();
    else if (query == RenderMatrixQuery::Texture)
        matrix = &pcD3D9State().textureMatrices[static_cast<std::size_t>(pcD3D9State().activeTextureUnit)].top();
    std::copy(matrix->values.begin(), matrix->values.end(), values);
}

const unsigned char* renderGetString(RenderStringQuery query)
{
    static const unsigned char vendor[] = "Microsoft Direct3D";
    static const unsigned char renderer[] = "Direct3D 9 fixed-function";
    static const unsigned char version[] = "Direct3D 9";
    static const unsigned char extensions[] = "";
    switch (query)
    {
        case RenderStringQuery::Vendor: return vendor;
        case RenderStringQuery::Renderer: return renderer;
        case RenderStringQuery::Version: return version;
        case RenderStringQuery::Extensions: return extensions;
    }
    return extensions;
}

bool renderSupportsFeature(RenderFeature feature)
{
    IDirect3DDevice9* device = pcD3D9Device();
    if (device == nullptr)
        return false;
    D3DCAPS9 caps{};
    device->GetDeviceCaps(&caps);
    switch (feature)
    {
        case RenderFeature::FancyFogDistance: return (caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) != 0;
        case RenderFeature::OcclusionQuery:
            // Keep D3D9 on the same CPU-culling path as the Legacy profile.
            // D3D9 queries are reset-sensitive and add synchronization overhead
            // on the low-end Intel hardware this backend targets.
            return false;
        case RenderFeature::Mipmaps: return (caps.TextureCaps & D3DPTEXTURECAPS_MIPMAP) != 0;
        case RenderFeature::AnisotropicFiltering: return renderGetMaxAnisotropy() > 1;
        case RenderFeature::MultisampleAntialiasing: return renderGetMaxSamples() > 0;
    }
    return false;
}

unsigned int renderGetError()
{
    return 0;
}

void renderFogHint(RenderHintMode)
{
}

void renderMatrixMode(RenderMatrixMode mode)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordMatrixMode(mode);
        return;
    }
    pcD3D9State().matrixMode = mode;
}

void renderLoadIdentity()
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordLoadIdentity();
        return;
    }
    pcD3D9CurrentMatrixStack().top() = pcD3D9MatrixIdentity();
    pcD3D9MarkCurrentMatrixDirty();
}

void renderPushMatrix()
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordPushMatrix();
        return;
    }
    auto& stack = pcD3D9CurrentMatrixStack();
    stack.values.push_back(stack.top());
    pcD3D9MarkCurrentMatrixDirty();
}

void renderPopMatrix()
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordPopMatrix();
        return;
    }
    auto& stack = pcD3D9CurrentMatrixStack();
    if (stack.values.size() > 1)
    {
        stack.values.pop_back();
        pcD3D9MarkCurrentMatrixDirty();
    }
}

void renderTranslate(float x, float y, float z)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordTranslate(x, y, z);
        return;
    }
    auto& matrix = pcD3D9CurrentMatrixStack().top();
    matrix = appended(matrix, pcD3D9MatrixTranslation(x, y, z));
    pcD3D9MarkCurrentMatrixDirty();
}

void renderRotate(float angle, float x, float y, float z)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordRotate(angle, x, y, z);
        return;
    }
    auto& matrix = pcD3D9CurrentMatrixStack().top();
    matrix = appended(matrix, pcD3D9MatrixRotation(angle, x, y, z));
    pcD3D9MarkCurrentMatrixDirty();
}

void renderScale(float x, float y, float z)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordScale(x, y, z);
        return;
    }
    auto& matrix = pcD3D9CurrentMatrixStack().top();
    matrix = appended(matrix, pcD3D9MatrixScale(x, y, z));
    pcD3D9MarkCurrentMatrixDirty();
}

void renderScaleDouble(double x, double y, double z)
{
    renderScale(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

void renderFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordFrustum(left, right, bottom, top, nearValue, farValue);
        return;
    }
    auto& matrix = pcD3D9CurrentMatrixStack().top();
    matrix = appended(matrix, pcD3D9MatrixFrustum(left, right, bottom, top, nearValue, farValue));
    pcD3D9MarkCurrentMatrixDirty();
}

void renderOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordOrtho(left, right, bottom, top, nearValue, farValue);
        return;
    }
    auto& matrix = pcD3D9CurrentMatrixStack().top();
    matrix = appended(matrix, pcD3D9MatrixOrtho(left, right, bottom, top, nearValue, farValue));
    pcD3D9MarkCurrentMatrixDirty();
}

bool renderDrawInterleaved(const RenderInterleavedMesh& mesh)
{
    if (mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0)
        return false;
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordDraw(mesh);
        return true;
    }
    if (pcD3D9State().cullEnabled && pcD3D9State().cullAll)
        return true;

    static PcD3D9PreparedMesh prepared;
    const float* fallbackNormal = pcD3D9State().lightingEnabled ? pcD3D9State().currentNormal.data() : nullptr;
    if (!pcD3D9PrepareMesh(mesh, fallbackNormal, prepared))
        return false;
    pcD3D9ApplyDrawState(prepared);
    return pcD3D9DrawPreparedMesh(pcD3D9Device(), prepared);
}

bool renderCaptureInterleaved(const RenderInterleavedMesh& mesh, RenderCapturedMesh& out, bool append)
{
    if (mesh.data == nullptr || mesh.stride <= 0 || mesh.count <= 0)
        return false;
    if (!append)
        out.clear();
    if (!out.empty() && (out.stride != mesh.stride || out.primitive != mesh.primitive ||
        out.positionShort != mesh.positionShort ||
        out.hasTexture != mesh.hasTexture || (mesh.hasTexture && out.texCoordOffset != mesh.texCoordOffset) ||
        out.hasColor != mesh.hasColor || (mesh.hasColor && out.colorOffset != mesh.colorOffset) ||
        out.hasNormals != mesh.hasNormals || (mesh.hasNormals && out.normalOffset != mesh.normalOffset) ||
        out.hasBrightness != mesh.hasBrightness || (mesh.hasBrightness && out.brightnessOffset != mesh.brightnessOffset)))
        return false;

    if (out.empty())
    {
        out.stride = mesh.stride;
        out.primitive = mesh.primitive;
        out.positionShort = mesh.positionShort;
        out.hasTexture = mesh.hasTexture;
        out.texCoordOffset = mesh.texCoordOffset;
        out.hasColor = mesh.hasColor;
        out.colorOffset = mesh.colorOffset;
        out.hasNormals = mesh.hasNormals;
        out.normalOffset = mesh.normalOffset;
        out.hasBrightness = mesh.hasBrightness;
        out.brightnessOffset = mesh.brightnessOffset;
    }

    const unsigned char* source = static_cast<const unsigned char*>(mesh.data) +
                                  static_cast<std::size_t>(mesh.first) * static_cast<std::size_t>(mesh.stride);
    const std::size_t bytes = static_cast<std::size_t>(mesh.count) * static_cast<std::size_t>(mesh.stride);
    const std::size_t oldWords = out.raw.size();
    out.raw.resize(oldWords + (bytes + 3u) / 4u);
    std::memcpy(reinterpret_cast<unsigned char*>(out.raw.data()) + oldWords * 4u, source, bytes);
    out.vertexCount += mesh.count;
    return true;
}

bool renderDrawCaptured(const RenderCapturedMesh& mesh)
{
    if (mesh.empty())
        return false;
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordCapturedDraw(mesh);
        return true;
    }
    RenderInterleavedMesh view;
    view.data = mesh.raw.data();
    view.stride = mesh.stride;
    view.count = mesh.vertexCount;
    view.primitive = mesh.primitive;
    view.positionShort = mesh.positionShort;
    view.hasTexture = mesh.hasTexture;
    view.texCoordOffset = mesh.texCoordOffset;
    view.hasColor = mesh.hasColor;
    view.colorOffset = mesh.colorOffset;
    view.hasNormals = mesh.hasNormals;
    view.normalOffset = mesh.normalOffset;
    view.hasBrightness = mesh.hasBrightness;
    view.brightnessOffset = mesh.brightnessOffset;
    return PcD3D9RenderBackend::renderDrawInterleaved(view);
}
}

#endif
