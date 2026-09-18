#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <d3d9.h>

#include "pc/render/d3d9/PcD3D9Matrix.h"
#include "pc/render/d3d9/PcD3D9Mesh.h"
#include "platform/RenderAPI.h"

struct PcD3D9TextureRecord
{
    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;
    int maxLevel = 0;
    bool blur = false;
    bool clamp = false;

    ~PcD3D9TextureRecord();
    PcD3D9TextureRecord() = default;
    PcD3D9TextureRecord(const PcD3D9TextureRecord&) = delete;
    PcD3D9TextureRecord& operator=(const PcD3D9TextureRecord&) = delete;
    PcD3D9TextureRecord(PcD3D9TextureRecord&& other) noexcept;
    PcD3D9TextureRecord& operator=(PcD3D9TextureRecord&& other) noexcept;
};

struct PcD3D9MatrixStack
{
    std::vector<PcD3D9Matrix> values{pcD3D9MatrixIdentity()};

    PcD3D9Matrix& top() { return values.back(); }
    const PcD3D9Matrix& top() const { return values.back(); }
};

struct PcD3D9BackendState
{
    int activeTextureUnit = 0;
    int clientTextureUnit = 0;
    std::array<int, 2> boundTextureIds{{0, 0}};
    std::array<IDirect3DTexture9*, 2> boundTextures{{nullptr, nullptr}};
    std::array<bool, 2> textureEnabled{{false, false}};
    std::array<std::array<float, 2>, 2> currentTexCoord{{{{0.0f, 0.0f}}, {{0.0f, 0.0f}}}};

    RenderMatrixMode matrixMode = RenderMatrixMode::ModelView;
    PcD3D9MatrixStack modelView;
    PcD3D9MatrixStack projection;
    std::array<PcD3D9MatrixStack, 2> textureMatrices;

    std::array<float, 4> currentColor{{1.0f, 1.0f, 1.0f, 1.0f}};
    std::array<float, 3> currentNormal{{0.0f, 1.0f, 0.0f}};
    std::array<float, 4> clearColor{{0.0f, 0.0f, 0.0f, 0.0f}};
    double clearDepth = 1.0;
    std::array<int, 4> viewport{{0, 0, 1, 1}};

    bool lightingEnabled = false;
    std::array<D3DLIGHT9, 8> lights{};
    std::array<bool, 8> lightDefined{};
    std::array<bool, 8> lightEnabled{};
    bool colorMaterialEnabled = false;
    bool cullEnabled = false;
    bool cullAll = false;
    RenderFace cullFace = RenderFace::Back;
    float lineWidth = 1.0f;
    bool polygonOffsetEnabled = false;
    float polygonOffsetFactor = 0.0f;
    float polygonOffsetUnits = 0.0f;

    bool modelViewTransformDirty = true;
    bool projectionTransformDirty = true;
    std::array<bool, 2> textureTransformDirty{{true, true}};
    std::array<bool, 2> textureTransformModeValid{{false, false}};
    std::array<bool, 2> textureTransformUsesVertexCoords{{false, false}};

    bool drawStateValid = false;
    bool drawHasTexture = false;
    bool drawHasColor = false;
    bool drawHasBrightness = false;
    bool drawHasNormals = false;

    std::array<IDirect3DBaseTexture9*, 2> appliedTextures{{nullptr, nullptr}};
    std::array<bool, 2> appliedTextureValid{{false, false}};
    std::array<DWORD, 256> renderStateValues{};
    std::array<bool, 256> renderStateValid{};
    std::array<std::array<DWORD, 64>, 2> textureStageStateValues{};
    std::array<std::array<bool, 64>, 2> textureStageStateValid{};
    std::array<std::array<DWORD, 16>, 2> samplerStateValues{};
    std::array<std::array<bool, 16>, 2> samplerStateValid{};
    DWORD appliedFvf = 0;
    bool appliedFvfValid = false;
    IDirect3DVertexBuffer9* appliedVertexBuffer = nullptr;
    UINT appliedVertexStride = 0;
    bool appliedVertexStreamValid = false;
    IDirect3DIndexBuffer9* appliedIndexBuffer = nullptr;
    bool appliedIndexBufferValid = false;

    int nextTextureId = 1;
    std::unordered_map<int, PcD3D9TextureRecord> textures;
};

PcD3D9BackendState& pcD3D9State();
IDirect3DDevice9* pcD3D9Device();
int pcD3D9TextureStageFromConstant(int textureUnit);
PcD3D9MatrixStack& pcD3D9CurrentMatrixStack();
void pcD3D9ApplyTransforms();
void pcD3D9ApplyTextureTransform(int stage, bool hasPerVertexCoordinates);
void pcD3D9ApplyDrawState(const PcD3D9RetainedMesh& mesh);
void pcD3D9ApplyDrawState(const PcD3D9PreparedMesh& mesh);
void pcD3D9RestoreDefaultState();
void pcD3D9CaptureStateForReset();
void pcD3D9RestoreStateAfterReset();
void pcD3D9ForceFixedFunctionPipeline();
void pcD3D9InvalidateDeviceStateCache();
void pcD3D9MarkDrawStateDirty();
void pcD3D9MarkCurrentMatrixDirty();
void pcD3D9MarkTextureTransformDirty(int stage);
bool pcD3D9SetRenderState(D3DRENDERSTATETYPE state, DWORD value);
bool pcD3D9SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value);
bool pcD3D9SetSamplerState(DWORD stage, D3DSAMPLERSTATETYPE state, DWORD value);
bool pcD3D9SetFvf(DWORD fvf);
bool pcD3D9SetVertexStream(IDirect3DVertexBuffer9* vertexBuffer, UINT stride);
bool pcD3D9SetIndexBuffer(IDirect3DIndexBuffer9* indexBuffer);
void pcD3D9InvalidateVertexBindings();
D3DCOLOR pcD3D9Color(float r, float g, float b, float a);
D3DCMPFUNC pcD3D9Compare(RenderCompare compare);
D3DBLEND pcD3D9Blend(RenderBlendFactor factor);

IDirect3DTexture9* pcD3D9LookupTexture(int textureId);
void pcD3D9BindTextureStage(int stage);
void pcD3D9ClearTextures();
void pcD3D9ClearQueries();

bool pcD3D9DisplayListRecording();
void pcD3D9RecordDraw(const RenderInterleavedMesh& mesh);
void pcD3D9RecordCapturedDraw(const RenderCapturedMesh& mesh);
void pcD3D9RecordCallList(int list);
void pcD3D9RecordBindTexture(int texture);
void pcD3D9RecordActiveTexture(int textureUnit);
void pcD3D9RecordMultiTextureCoord(int textureUnit, float u, float v);
void pcD3D9RecordColor(float r, float g, float b, float a);
void pcD3D9RecordNormal(float x, float y, float z);
void pcD3D9RecordMatrixMode(RenderMatrixMode mode);
void pcD3D9RecordLoadIdentity();
void pcD3D9RecordPushMatrix();
void pcD3D9RecordPopMatrix();
void pcD3D9RecordTranslate(float x, float y, float z);
void pcD3D9RecordRotate(float angle, float x, float y, float z);
void pcD3D9RecordScale(float x, float y, float z);
void pcD3D9RecordFrustum(double left, double right, double bottom, double top, double nearValue, double farValue);
void pcD3D9RecordOrtho(double left, double right, double bottom, double top, double nearValue, double farValue);
void pcD3D9ClearDisplayLists();

#endif
