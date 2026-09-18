#include "pc/render/PcRenderBackendApi.h"

#if PLATFORM_PC && defined(MC_WIN32)

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

#include "pc/render/d3d9/PcD3D9Internal.h"

namespace
{
enum class CommandType
{
    DrawMesh,
    CallList,
    BindTexture,
    ActiveTexture,
    MultiTextureCoord,
    Color,
    Normal,
    MatrixMode,
    LoadIdentity,
    PushMatrix,
    PopMatrix,
    Translate,
    Rotate,
    Scale,
    Frustum,
    Ortho
};

struct DisplayCommand
{
    CommandType type = CommandType::DrawMesh;
    int integer = 0;
    std::array<float, 4> values{{0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<double, 6> doubles{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    std::shared_ptr<PcD3D9RetainedMesh> mesh;
};

struct DisplayListRecord
{
    std::vector<DisplayCommand> commands;
    PcD3D9Matrix staticModelView = pcD3D9MatrixIdentity();
    std::size_t fastBodyBegin = 0;
    std::size_t fastBodyEnd = 0;
    std::uint32_t revision = 0;
    bool staticTransformFastPath = false;
    bool staticTransformBaked = false;
    bool simpleTerrainMergeCandidate = false;
    std::shared_ptr<PcD3D9RetainedMesh> simpleTerrainMesh;
};

struct MergedTerrainBatch
{
    std::vector<int> lists;
    std::vector<std::uint32_t> revisions;
    std::shared_ptr<PcD3D9RetainedMesh> mesh;
    std::size_t bytes = 0;
    std::uint64_t lastUse = 0;
};

std::vector<DisplayListRecord> g_lists(1);
std::vector<MergedTerrainBatch> g_mergedTerrainBatches;
int g_nextList = 1;
int g_recordingList = 0;
std::uint32_t g_nextRevision = 1;
std::uint64_t g_mergedTerrainUseCounter = 1;
std::size_t g_mergedTerrainCacheBytes = 0;

constexpr int kMinMergedTerrainLists = 4;
constexpr std::size_t kMaxMergedTerrainCacheEntries = 12u;
constexpr std::size_t kMaxMergedTerrainBatchBytes = 3u * 1024u * 1024u;
constexpr std::size_t kMaxMergedTerrainCacheBytes = 12u * 1024u * 1024u;

DisplayListRecord* listRecord(int list)
{
    if (list <= 0 || static_cast<std::size_t>(list) >= g_lists.size())
        return nullptr;
    return &g_lists[static_cast<std::size_t>(list)];
}

const DisplayListRecord* listRecordConst(int list)
{
    if (list <= 0 || static_cast<std::size_t>(list) >= g_lists.size())
        return nullptr;
    return &g_lists[static_cast<std::size_t>(list)];
}

std::vector<DisplayCommand>* listCommands(int list)
{
    DisplayListRecord* record = listRecord(list);
    return record != nullptr ? &record->commands : nullptr;
}

std::vector<DisplayCommand>* recordingCommands()
{
    return listCommands(g_recordingList);
}

void appendCommand(DisplayCommand command)
{
    if (std::vector<DisplayCommand>* commands = recordingCommands())
        commands->push_back(std::move(command));
}

std::shared_ptr<PcD3D9RetainedMesh> retainMesh(const RenderInterleavedMesh& mesh)
{
    return pcD3D9CreateRetainedMesh(pcD3D9Device(), mesh, nullptr);
}


PcD3D9Matrix commandTransform(const DisplayCommand& command)
{
    switch (command.type)
    {
        case CommandType::Translate:
            return pcD3D9MatrixTranslation(command.values[0], command.values[1], command.values[2]);
        case CommandType::Rotate:
            return pcD3D9MatrixRotation(command.values[0], command.values[1], command.values[2], command.values[3]);
        case CommandType::Scale:
            return pcD3D9MatrixScale(command.values[0], command.values[1], command.values[2]);
        default:
            return pcD3D9MatrixIdentity();
    }
}

bool isStaticTransformCommand(CommandType type)
{
    return type == CommandType::Translate || type == CommandType::Rotate || type == CommandType::Scale;
}

bool isFastBodyCommand(CommandType type)
{
    return type == CommandType::DrawMesh || type == CommandType::BindTexture;
}

bool isTerrainMesh(const std::shared_ptr<PcD3D9RetainedMesh>& mesh)
{
    return mesh != nullptr && mesh->vertexBuffer != nullptr && mesh->indexedQuads &&
           mesh->hasTexture && mesh->hasBrightness && !mesh->hasNormals &&
           mesh->vertexCount >= 4 && (mesh->vertexCount & 3) == 0;
}

void clearMergedTerrainCache()
{
    if (!g_mergedTerrainBatches.empty())
        pcD3D9InvalidateVertexBindings();
    g_mergedTerrainBatches.clear();
    g_mergedTerrainCacheBytes = 0;
    g_mergedTerrainUseCounter = 1;
}

bool bakeTerrainStaticTransform(DisplayListRecord& record)
{
    if (!record.staticTransformFastPath)
        return false;

    std::vector<std::pair<std::size_t, std::shared_ptr<PcD3D9RetainedMesh>>> replacements;
    replacements.reserve(record.fastBodyEnd - record.fastBodyBegin);

    for (std::size_t index = record.fastBodyBegin; index < record.fastBodyEnd; ++index)
    {
        const DisplayCommand& command = record.commands[index];
        if (command.type != CommandType::DrawMesh)
            continue;
        if (!isTerrainMesh(command.mesh))
            return false;

        std::shared_ptr<PcD3D9RetainedMesh> transformed =
            pcD3D9CreateTransformedRetainedMesh(pcD3D9Device(), *command.mesh, record.staticModelView);
        if (!transformed)
            return false;
        replacements.emplace_back(index, std::move(transformed));
    }

    if (replacements.empty())
        return false;

    for (auto& replacement : replacements)
        record.commands[replacement.first].mesh = std::move(replacement.second);

    record.staticTransformBaked = true;
    record.simpleTerrainMergeCandidate = record.fastBodyEnd == record.fastBodyBegin + 1 &&
        record.commands[record.fastBodyBegin].type == CommandType::DrawMesh;
    if (record.simpleTerrainMergeCandidate)
        record.simpleTerrainMesh = record.commands[record.fastBodyBegin].mesh;
    return true;
}

void optimizeStaticTransform(DisplayListRecord& record)
{
    record.staticTransformFastPath = false;
    record.staticModelView = pcD3D9MatrixIdentity();
    record.fastBodyBegin = 0;
    record.fastBodyEnd = 0;

    const std::vector<DisplayCommand>& commands = record.commands;
    if (commands.size() < 4 || commands.front().type != CommandType::PushMatrix ||
        commands.back().type != CommandType::PopMatrix)
        return;

    std::size_t index = 1;
    bool hasTransform = false;
    while (index + 1 < commands.size() && isStaticTransformCommand(commands[index].type))
    {
        record.staticModelView = pcD3D9MatrixMultiply(record.staticModelView, commandTransform(commands[index]));
        hasTransform = true;
        ++index;
    }

    if (!hasTransform || index + 1 >= commands.size())
        return;

    bool hasDraw = false;
    for (std::size_t bodyIndex = index; bodyIndex + 1 < commands.size(); ++bodyIndex)
    {
        if (!isFastBodyCommand(commands[bodyIndex].type))
            return;
        hasDraw |= commands[bodyIndex].type == CommandType::DrawMesh;
    }
    if (!hasDraw)
        return;

    record.fastBodyBegin = index;
    record.fastBodyEnd = commands.size() - 1;
    record.staticTransformFastPath = true;
    bakeTerrainStaticTransform(record);
}

RenderInterleavedMesh capturedView(const RenderCapturedMesh& mesh)
{
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
    return view;
}

void execute(const DisplayCommand& command)
{
    switch (command.type)
    {
        case CommandType::DrawMesh:
            if (command.mesh)
            {
                pcD3D9ApplyDrawState(*command.mesh);
                pcD3D9DrawRetainedMesh(pcD3D9Device(), *command.mesh);
            }
            break;
        case CommandType::CallList: PcD3D9RenderBackend::renderCallDisplayList(command.integer); break;
        case CommandType::BindTexture: PcD3D9RenderBackend::renderBindTexture(command.integer); break;
        case CommandType::ActiveTexture: PcD3D9RenderBackend::renderSetActiveTextureUnit(command.integer); break;
        case CommandType::MultiTextureCoord: PcD3D9RenderBackend::renderSetMultiTextureCoord(command.integer, command.values[0], command.values[1]); break;
        case CommandType::Color: PcD3D9RenderBackend::renderColor4f(command.values[0], command.values[1], command.values[2], command.values[3]); break;
        case CommandType::Normal: PcD3D9RenderBackend::renderNormal3f(command.values[0], command.values[1], command.values[2]); break;
        case CommandType::MatrixMode: PcD3D9RenderBackend::renderMatrixMode(static_cast<RenderMatrixMode>(command.integer)); break;
        case CommandType::LoadIdentity: PcD3D9RenderBackend::renderLoadIdentity(); break;
        case CommandType::PushMatrix: PcD3D9RenderBackend::renderPushMatrix(); break;
        case CommandType::PopMatrix: PcD3D9RenderBackend::renderPopMatrix(); break;
        case CommandType::Translate: PcD3D9RenderBackend::renderTranslate(command.values[0], command.values[1], command.values[2]); break;
        case CommandType::Rotate: PcD3D9RenderBackend::renderRotate(command.values[0], command.values[1], command.values[2], command.values[3]); break;
        case CommandType::Scale: PcD3D9RenderBackend::renderScale(command.values[0], command.values[1], command.values[2]); break;
        case CommandType::Frustum:
            PcD3D9RenderBackend::renderFrustum(command.doubles[0], command.doubles[1], command.doubles[2], command.doubles[3], command.doubles[4], command.doubles[5]);
            break;
        case CommandType::Ortho:
            PcD3D9RenderBackend::renderOrtho(command.doubles[0], command.doubles[1], command.doubles[2], command.doubles[3], command.doubles[4], command.doubles[5]);
            break;
    }
}

void executeGeneric(const DisplayListRecord& record)
{
    for (const DisplayCommand& command : record.commands)
        execute(command);
}

void executeStaticTransformBody(const DisplayListRecord& record)
{
    for (std::size_t index = record.fastBodyBegin; index < record.fastBodyEnd; ++index)
        execute(record.commands[index]);
}

bool executeStaticTransformFastPath(const DisplayListRecord& record)
{
    if (!record.staticTransformFastPath)
        return false;

    PcD3D9BackendState& state = pcD3D9State();
    if (state.matrixMode != RenderMatrixMode::ModelView || state.modelView.values.empty())
        return false;

    PcD3D9Matrix& current = state.modelView.top();
    const PcD3D9Matrix saved = current;
    current = pcD3D9MatrixMultiplyAffine(saved, record.staticModelView);
    pcD3D9MarkCurrentMatrixDirty();

    executeStaticTransformBody(record);

    current = saved;
    pcD3D9MarkCurrentMatrixDirty();
    return true;
}

void executeDisplayList(const DisplayListRecord& record)
{
    if (record.staticTransformBaked)
    {
        executeStaticTransformBody(record);
        return;
    }
    if (!executeStaticTransformFastPath(record))
        executeGeneric(record);
}

bool allStaticTransformFastPath(int count, const int* lists)
{
    if (count <= 0 || lists == nullptr)
        return false;

    const PcD3D9BackendState& state = pcD3D9State();
    if (state.matrixMode != RenderMatrixMode::ModelView || state.modelView.values.empty())
        return false;

    for (int i = 0; i < count; ++i)
    {
        const DisplayListRecord* record = listRecordConst(lists[i]);
        if (record == nullptr || !record->staticTransformFastPath || record->staticTransformBaked)
            return false;
    }
    return true;
}

bool executeStaticTransformBatch(int count, const int* lists)
{
    if (!allStaticTransformFastPath(count, lists))
        return false;

    PcD3D9BackendState& state = pcD3D9State();
    PcD3D9Matrix& current = state.modelView.top();
    const PcD3D9Matrix outerModelView = current;

    for (int i = 0; i < count; ++i)
    {
        const DisplayListRecord& record = *listRecordConst(lists[i]);
        current = pcD3D9MatrixMultiplyAffine(outerModelView, record.staticModelView);
        pcD3D9MarkCurrentMatrixDirty();
        executeStaticTransformBody(record);
    }

    current = outerModelView;
    pcD3D9MarkCurrentMatrixDirty();
    return true;
}


const PcD3D9RetainedMesh* simpleTerrainMesh(const DisplayListRecord* record)
{
    if (record == nullptr || !record->staticTransformBaked || !record->simpleTerrainMergeCandidate ||
        !isTerrainMesh(record->simpleTerrainMesh))
        return nullptr;
    return record->simpleTerrainMesh.get();
}

std::size_t terrainMeshBytes(const PcD3D9RetainedMesh& mesh)
{
    return static_cast<std::size_t>(mesh.vertexCount) * mesh.stride;
}

bool sameMergedBatchKey(const MergedTerrainBatch& batch, int begin, int end, const int* lists)
{
    const int count = end - begin;
    if (count <= 0 || batch.lists.size() != static_cast<std::size_t>(count) ||
        batch.revisions.size() != static_cast<std::size_t>(count))
        return false;

    for (int offset = 0; offset < count; ++offset)
    {
        const int list = lists[begin + offset];
        const DisplayListRecord* record = listRecordConst(list);
        if (record == nullptr || batch.lists[static_cast<std::size_t>(offset)] != list ||
            batch.revisions[static_cast<std::size_t>(offset)] != record->revision)
            return false;
    }
    return true;
}

void evictMergedTerrainCacheFor(std::size_t incomingBytes)
{
    while (!g_mergedTerrainBatches.empty() &&
           (g_mergedTerrainCacheBytes + incomingBytes > kMaxMergedTerrainCacheBytes ||
            g_mergedTerrainBatches.size() >= kMaxMergedTerrainCacheEntries))
    {
        std::size_t oldestIndex = 0;
        for (std::size_t index = 1; index < g_mergedTerrainBatches.size(); ++index)
        {
            if (g_mergedTerrainBatches[index].lastUse < g_mergedTerrainBatches[oldestIndex].lastUse)
                oldestIndex = index;
        }
        pcD3D9InvalidateVertexBindings();
        g_mergedTerrainCacheBytes -= g_mergedTerrainBatches[oldestIndex].bytes;
        g_mergedTerrainBatches.erase(g_mergedTerrainBatches.begin() + static_cast<std::ptrdiff_t>(oldestIndex));
    }
}

std::shared_ptr<PcD3D9RetainedMesh> mergedTerrainBatch(int begin, int end, const int* lists)
{
    const int count = end - begin;
    if (count < kMinMergedTerrainLists)
        return {};

    for (MergedTerrainBatch& batch : g_mergedTerrainBatches)
    {
        if (sameMergedBatchKey(batch, begin, end, lists))
        {
            batch.lastUse = g_mergedTerrainUseCounter++;
            return batch.mesh;
        }
    }

    std::vector<const PcD3D9RetainedMesh*> meshes;
    meshes.reserve(static_cast<std::size_t>(count));
    std::size_t totalBytes = 0;
    for (int index = begin; index < end; ++index)
    {
        const PcD3D9RetainedMesh* mesh = simpleTerrainMesh(listRecordConst(lists[index]));
        if (mesh == nullptr)
            return {};
        totalBytes += terrainMeshBytes(*mesh);
        if (totalBytes > kMaxMergedTerrainBatchBytes)
            return {};
        meshes.push_back(mesh);
    }

    std::shared_ptr<PcD3D9RetainedMesh> merged = pcD3D9CreateMergedRetainedMesh(pcD3D9Device(), meshes);
    if (!merged)
        return {};

    evictMergedTerrainCacheFor(totalBytes);
    if (totalBytes > kMaxMergedTerrainCacheBytes)
        return {};

    MergedTerrainBatch batch;
    batch.lists.reserve(static_cast<std::size_t>(count));
    batch.revisions.reserve(static_cast<std::size_t>(count));
    for (int index = begin; index < end; ++index)
    {
        const int list = lists[index];
        const DisplayListRecord* record = listRecordConst(list);
        batch.lists.push_back(list);
        batch.revisions.push_back(record != nullptr ? record->revision : 0);
    }
    batch.mesh = merged;
    batch.bytes = totalBytes;
    batch.lastUse = g_mergedTerrainUseCounter++;
    g_mergedTerrainCacheBytes += totalBytes;
    g_mergedTerrainBatches.push_back(std::move(batch));
    return merged;
}

void executeTerrainBatch(int count, const int* lists)
{
    int index = 0;
    while (index < count)
    {
        const DisplayListRecord* record = listRecordConst(lists[index]);
        if (simpleTerrainMesh(record) == nullptr)
        {
            if (record != nullptr)
                executeDisplayList(*record);
            ++index;
            continue;
        }

        int runEnd = index;
        std::size_t runBytes = 0;
        while (runEnd < count)
        {
            const PcD3D9RetainedMesh* runMesh = simpleTerrainMesh(listRecordConst(lists[runEnd]));
            if (runMesh == nullptr)
                break;
            const std::size_t meshBytes = terrainMeshBytes(*runMesh);
            if (runEnd > index && runBytes + meshBytes > kMaxMergedTerrainBatchBytes)
                break;
            runBytes += meshBytes;
            ++runEnd;
        }

        std::shared_ptr<PcD3D9RetainedMesh> merged = mergedTerrainBatch(index, runEnd, lists);
        if (merged)
        {
            pcD3D9ApplyDrawState(*merged);
            pcD3D9DrawRetainedMesh(pcD3D9Device(), *merged);
        }
        else
        {
            for (int runIndex = index; runIndex < runEnd; ++runIndex)
            {
                const DisplayListRecord* runRecord = listRecordConst(lists[runIndex]);
                if (runRecord != nullptr)
                    executeDisplayList(*runRecord);
            }
        }
        index = runEnd;
    }
}
}

bool pcD3D9DisplayListRecording()
{
    return g_recordingList > 0;
}

void pcD3D9RecordDraw(const RenderInterleavedMesh& mesh)
{
    DisplayCommand command;
    command.type = CommandType::DrawMesh;
    command.mesh = retainMesh(mesh);
    if (command.mesh)
        appendCommand(std::move(command));
}

void pcD3D9RecordCapturedDraw(const RenderCapturedMesh& mesh)
{
    if (mesh.empty())
        return;
    pcD3D9RecordDraw(capturedView(mesh));
}

void pcD3D9RecordCallList(int list)
{
    DisplayCommand command;
    command.type = CommandType::CallList;
    command.integer = list;
    appendCommand(std::move(command));
}

void pcD3D9RecordBindTexture(int texture)
{
    DisplayCommand command;
    command.type = CommandType::BindTexture;
    command.integer = texture;
    appendCommand(std::move(command));
}

void pcD3D9RecordActiveTexture(int textureUnit)
{
    DisplayCommand command;
    command.type = CommandType::ActiveTexture;
    command.integer = textureUnit;
    appendCommand(std::move(command));
}

void pcD3D9RecordMultiTextureCoord(int textureUnit, float u, float v)
{
    DisplayCommand command;
    command.type = CommandType::MultiTextureCoord;
    command.integer = textureUnit;
    command.values[0] = u;
    command.values[1] = v;
    appendCommand(std::move(command));
}

void pcD3D9RecordColor(float r, float g, float b, float a)
{
    DisplayCommand command;
    command.type = CommandType::Color;
    command.values = {{r, g, b, a}};
    appendCommand(std::move(command));
}

void pcD3D9RecordNormal(float x, float y, float z)
{
    DisplayCommand command;
    command.type = CommandType::Normal;
    command.values = {{x, y, z, 0.0f}};
    appendCommand(std::move(command));
}

void pcD3D9RecordMatrixMode(RenderMatrixMode mode)
{
    DisplayCommand command;
    command.type = CommandType::MatrixMode;
    command.integer = static_cast<int>(mode);
    appendCommand(std::move(command));
}

void pcD3D9RecordLoadIdentity()
{
    DisplayCommand command;
    command.type = CommandType::LoadIdentity;
    appendCommand(std::move(command));
}

void pcD3D9RecordPushMatrix()
{
    DisplayCommand command;
    command.type = CommandType::PushMatrix;
    appendCommand(std::move(command));
}

void pcD3D9RecordPopMatrix()
{
    DisplayCommand command;
    command.type = CommandType::PopMatrix;
    appendCommand(std::move(command));
}

void pcD3D9RecordTranslate(float x, float y, float z)
{
    DisplayCommand command;
    command.type = CommandType::Translate;
    command.values = {{x, y, z, 0.0f}};
    appendCommand(std::move(command));
}

void pcD3D9RecordRotate(float angle, float x, float y, float z)
{
    DisplayCommand command;
    command.type = CommandType::Rotate;
    command.values = {{angle, x, y, z}};
    appendCommand(std::move(command));
}

void pcD3D9RecordScale(float x, float y, float z)
{
    DisplayCommand command;
    command.type = CommandType::Scale;
    command.values = {{x, y, z, 0.0f}};
    appendCommand(std::move(command));
}

void pcD3D9RecordFrustum(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    DisplayCommand command;
    command.type = CommandType::Frustum;
    command.doubles = {{left, right, bottom, top, nearValue, farValue}};
    appendCommand(std::move(command));
}

void pcD3D9RecordOrtho(double left, double right, double bottom, double top, double nearValue, double farValue)
{
    DisplayCommand command;
    command.type = CommandType::Ortho;
    command.doubles = {{left, right, bottom, top, nearValue, farValue}};
    appendCommand(std::move(command));
}

void pcD3D9ClearDisplayLists()
{
    clearMergedTerrainCache();
    g_lists.clear();
    g_lists.resize(1);
    g_recordingList = 0;
    g_nextList = 1;
    g_nextRevision = 1;
}

namespace PcD3D9RenderBackend
{
int renderGenerateDisplayLists(int count)
{
    if (count <= 0)
        return 0;
    const int first = g_nextList;
    g_nextList += count;
    if (static_cast<std::size_t>(g_nextList) > g_lists.size())
        g_lists.resize(static_cast<std::size_t>(g_nextList));
    for (int i = 0; i < count; ++i)
        g_lists[static_cast<std::size_t>(first + i)] = DisplayListRecord{};
    return first;
}

void renderDeleteDisplayLists(int first, int count)
{
    if (count <= 0)
        return;
    for (int i = 0; i < count; ++i)
    {
        if (DisplayListRecord* record = listRecord(first + i))
        {
            record->commands.clear();
            record->commands.shrink_to_fit();
            record->staticTransformFastPath = false;
            record->staticTransformBaked = false;
            record->simpleTerrainMergeCandidate = false;
            record->simpleTerrainMesh.reset();
            record->fastBodyBegin = 0;
            record->fastBodyEnd = 0;
        }
    }
    clearMergedTerrainCache();
    if (g_recordingList >= first && g_recordingList < first + count)
        g_recordingList = 0;
}

void renderBeginDisplayList(int list)
{
    if (list <= 0)
        return;
    if (static_cast<std::size_t>(list) >= g_lists.size())
        g_lists.resize(static_cast<std::size_t>(list) + 1u);
    DisplayListRecord& record = g_lists[static_cast<std::size_t>(list)];
    record.commands.clear();
    record.staticTransformFastPath = false;
    record.staticTransformBaked = false;
    record.simpleTerrainMergeCandidate = false;
    record.simpleTerrainMesh.reset();
    record.staticModelView = pcD3D9MatrixIdentity();
    record.fastBodyBegin = 0;
    record.fastBodyEnd = 0;
    g_recordingList = list;
}

void renderEndDisplayList()
{
    if (DisplayListRecord* record = listRecord(g_recordingList))
    {
        optimizeStaticTransform(*record);
        record->revision = g_nextRevision++;
        if (g_nextRevision == 0)
        {
            clearMergedTerrainCache();
            g_nextRevision = 1;
        }
    }
    g_recordingList = 0;
}

void renderCallDisplayList(int list)
{
    if (pcD3D9DisplayListRecording())
    {
        pcD3D9RecordCallList(list);
        return;
    }
    const DisplayListRecord* record = listRecordConst(list);
    if (record == nullptr)
        return;
    executeDisplayList(*record);
}

void renderCallDisplayLists(int count, const int* lists)
{
    if (count <= 0 || lists == nullptr)
        return;
    if (pcD3D9DisplayListRecording())
    {
        for (int i = 0; i < count; ++i)
            pcD3D9RecordCallList(lists[i]);
        return;
    }

    if (executeStaticTransformBatch(count, lists))
        return;
    executeTerrainBatch(count, lists);
}
}

#endif
