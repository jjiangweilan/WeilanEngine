#include "TerrainPaintTool.hpp"
#include "TerrainPaintSmoothing.hpp"

#include "Editor/EditorState.hpp"
#include "Editor/Gizmos/GizmoManager.hpp"
#include "Editor/UndoManager.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/ThirdParty/imgui/ImGuizmo.h"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Editor
{
namespace
{
class TerrainHeightStrokeCommand final : public UndoCommand
{
public:
    TerrainHeightStrokeCommand(
        TerrainConfig* config,
        std::vector<TerrainHeightValue> before,
        std::vector<TerrainHeightValue> after
    )
        : config(config), before(std::move(before)), after(std::move(after)) {}

    void Undo() override { Apply(before); }
    void Redo() override { Apply(after); }
    const std::string& GetName() const override { return name; }

private:
    ObjPtr<TerrainConfig> config;
    std::vector<TerrainHeightValue> before;
    std::vector<TerrainHeightValue> after;
    std::string name = "Terrain Height Stroke";

    void Apply(const std::vector<TerrainHeightValue>& values)
    {
        TerrainConfig* target = config.Get();
        if (target == nullptr)
            return;
        target->ApplyHeightValues(values);
        target->CommitHeightData();
    }
};

class TerrainLayerStrokeCommand final : public UndoCommand
{
public:
    TerrainLayerStrokeCommand(
        TerrainConfig* config,
        std::vector<TerrainLayerControlValue> before,
        std::vector<TerrainLayerControlValue> after
    )
        : config(config), before(std::move(before)), after(std::move(after)) {}

    void Undo() override { Apply(before); }
    void Redo() override { Apply(after); }
    const std::string& GetName() const override { return name; }

private:
    ObjPtr<TerrainConfig> config;
    std::vector<TerrainLayerControlValue> before;
    std::vector<TerrainLayerControlValue> after;
    std::string name = "Terrain Layer Stroke";

    void Apply(const std::vector<TerrainLayerControlValue>& values)
    {
        TerrainConfig* target = config.Get();
        if (target == nullptr)
            return;
        target->ApplyLayerControlValues(values);
        target->CommitLayerControlData();
    }
};

struct TerrainBrushLine
{
    glm::vec4 fromPos;
    glm::vec4 toPos;
    glm::vec4 color;
};
} // namespace

void TerrainPaintTool::SetTargetTerrain(Terrain* terrain)
{
    FinishStroke();
    targetTerrain = terrain;
    selectedLayerID = TerrainConfig::InvalidTerrainLayerID;
    hasHit = false;
}

bool TerrainPaintTool::HasTarget() const
{
    Terrain* terrain = targetTerrain.Get();
    return terrain != nullptr && terrain->GetTerrainConfig() != nullptr &&
           terrain->GetTerrainConfig()->HasValidHeightData();
}

std::string TerrainPaintTool::GetTargetName() const
{
    Terrain* terrain = targetTerrain.Get();
    if (terrain == nullptr || terrain->GetGameObject() == nullptr)
        return "None";
    return terrain->GetGameObject()->GetName();
}

void TerrainPaintTool::SetBrushType(TerrainPaintBrushType value)
{
    if (brushType == value)
        return;
    FinishStroke();
    brushType = value;
}

void TerrainPaintTool::SetSelectedLayerID(uint32_t value)
{
    if (selectedLayerID == value)
        return;
    FinishStroke();
    selectedLayerID = value;
}

bool TerrainPaintTool::CanPaintSelectedLayer() const
{
    Terrain* terrain = targetTerrain.Get();
    TerrainConfig* config = terrain != nullptr ? terrain->GetTerrainConfig() : nullptr;
    if (config == nullptr || !config->HasValidLayerControlData() ||
        selectedLayerID >= TerrainConfig::MaxTerrainLayers)
        return false;
    return std::any_of(config->GetLayers().begin(), config->GetLayers().end(), [this](const TerrainLayer& layer)
    {
        return layer.id == selectedLayerID;
    });
}

bool TerrainPaintTool::Tick(const SceneEditorToolContext& ctx)
{
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        FinishStroke();

    if (!ctx.sceneViewHovered)
    {
        hasHit = false;
        return false;
    }
    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing() ||
        (ctx.gizmoManager != nullptr && ctx.gizmoManager->AnyGizmoActive()))
        return false;

    Terrain* terrain = targetTerrain.Get();
    if (!HasTarget())
    {
        hasHit = false;
        return false;
    }

    float distance = 0.0f;
    hasHit = terrain->Raycast(ctx.worldRay, distance, lastHitPoint, lastHitNormal);
    if (!hasHit)
        return true;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        isPainting = true;
        strokeBefore.clear();
        strokeWorkingHeights.clear();
        layerStrokeBefore.clear();
        layerStrokeWorking.clear();
    }

    if (isPainting && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const float direction = ImGui::GetIO().KeyShift ? -1.0f : 1.0f;
        ApplyBrush(lastHitPoint, direction, glm::min(ImGui::GetIO().DeltaTime, 0.05f));
    }
    return true;
}

void TerrainPaintTool::ApplyBrush(const float3& worldPoint, float direction, float deltaTime)
{
    if (brushType == TerrainPaintBrushType::Layer)
    {
        ApplyLayerBrush(worldPoint, deltaTime);
        return;
    }

    Terrain* terrain = targetTerrain.Get();
    TerrainConfig* config = terrain != nullptr ? terrain->GetTerrainConfig() : nullptr;
    GameObject* owner = terrain != nullptr ? terrain->GetGameObject() : nullptr;
    if (config == nullptr || owner == nullptr || brushRadius <= 0.0f || deltaTime <= 0.0f)
        return;

    const glm::mat4 model = owner->GetWorldMatrix();
    const glm::mat4 inverseModel = glm::inverse(model);
    const glm::vec3 localHit = inverseModel * glm::vec4(worldPoint, 1.0f);
    const float2 size = config->GetSize();
    const float2 heightRange = config->GetHeightRange();
    const uint32_t resolution = config->GetHeightMapResolution();
    const std::span<const uint16_t> samples = config->GetHeightSamples();
    const float worldYScale = glm::length(glm::vec3(model[1]));
    if (brushType == TerrainPaintBrushType::Height && worldYScale <= 0.0f)
        return;

    const float worldXScale = glm::length(glm::vec3(model[0]));
    const float worldZScale = glm::length(glm::vec3(model[2]));
    if (worldXScale <= 0.0f || worldZScale <= 0.0f)
        return;
    const float localRadiusX = brushRadius / worldXScale;
    const float localRadiusZ = brushRadius / worldZScale;
    const float texelExtent = static_cast<float>(resolution - 1);
    const float brushRadiusTexelsX = localRadiusX / size.x * texelExtent;
    const float brushRadiusTexelsZ = localRadiusZ / size.y * texelExtent;
    const float smoothStepX = TerrainPaintSmoothing::CalculateAdaptiveKernelStep(
        resolution,
        config->GetVertexResolution(),
        brushRadiusTexelsX
    );
    const float smoothStepZ = TerrainPaintSmoothing::CalculateAdaptiveKernelStep(
        resolution,
        config->GetVertexResolution(),
        brushRadiusTexelsZ
    );
    const float centerU = localHit.x / size.x + 0.5f;
    const float centerV = localHit.z / size.y + 0.5f;
    const int minX = glm::clamp(
        static_cast<int>(std::floor((centerU - localRadiusX / size.x) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int maxX = glm::clamp(
        static_cast<int>(std::ceil((centerU + localRadiusX / size.x) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int minY = glm::clamp(
        static_cast<int>(std::floor((centerV - localRadiusZ / size.y) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int maxY = glm::clamp(
        static_cast<int>(std::ceil((centerV + localRadiusZ / size.y) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );

    std::vector<TerrainHeightValue> changes;
    changes.reserve(static_cast<size_t>(maxX - minX + 1) * (maxY - minY + 1));
    const float normalizedScale = brushType == TerrainPaintBrushType::Height
                                      ? 65535.0f / ((heightRange.y - heightRange.x) * worldYScale)
                                      : 0.0f;
    for (int y = minY; y <= maxY; ++y)
    {
        const float v = static_cast<float>(y) / static_cast<float>(resolution - 1);
        const float localZ = (v - 0.5f) * size.y;
        for (int x = minX; x <= maxX; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
            const float localX = (u - 0.5f) * size.x;
            const glm::vec3 worldOffset = model * glm::vec4(localX - localHit.x, 0.0f, localZ - localHit.z, 0.0f);
            const float radialDistance = glm::length(worldOffset);
            if (radialDistance > brushRadius)
                continue;

            const float weight = std::pow(1.0f - radialDistance / brushRadius, brushFalloff);
            const uint32_t index = static_cast<uint32_t>(y) * resolution + static_cast<uint32_t>(x);
            const uint16_t oldValue = samples[index];
            uint16_t newValue = oldValue;
            if (brushType == TerrainPaintBrushType::Height)
            {
                const int delta = static_cast<int>(std::lround(
                    direction * brushStrength * deltaTime * weight * normalizedScale
                ));
                newValue = static_cast<uint16_t>(glm::clamp(static_cast<int>(oldValue) + delta, 0, 65535));
            }
            else
            {
                const float smoothedHeight = TerrainPaintSmoothing::CalculateSmoothedHeight(
                    samples,
                    resolution,
                    static_cast<float>(x),
                    static_cast<float>(y),
                    smoothStepX,
                    smoothStepZ
                );
                auto workingHeight = strokeWorkingHeights.try_emplace(index, static_cast<float>(oldValue)).first;
                workingHeight->second = TerrainPaintSmoothing::IntegrateSmoothedHeight(
                    workingHeight->second,
                    smoothedHeight,
                    smoothStrength,
                    deltaTime,
                    weight
                );
                newValue = static_cast<uint16_t>(std::lround(workingHeight->second));
            }
            if (newValue == oldValue)
                continue;
            strokeBefore.try_emplace(index, oldValue);
            changes.push_back({index, newValue});
        }
    }
    config->ApplyHeightValues(changes);
}

void TerrainPaintTool::ApplyLayerBrush(const float3& worldPoint, float deltaTime)
{
    Terrain* terrain = targetTerrain.Get();
    TerrainConfig* config = terrain != nullptr ? terrain->GetTerrainConfig() : nullptr;
    GameObject* owner = terrain != nullptr ? terrain->GetGameObject() : nullptr;
    if (config == nullptr || owner == nullptr || !CanPaintSelectedLayer() || brushRadius <= 0.0f ||
        layerStrength <= 0.0f || deltaTime <= 0.0f)
        return;

    const glm::mat4 model = owner->GetWorldMatrix();
    const glm::mat4 inverseModel = glm::inverse(model);
    const glm::vec3 localHit = inverseModel * glm::vec4(worldPoint, 1.0f);
    const float2 size = config->GetSize();
    const uint32_t resolution = config->GetLayerControlResolution();
    const std::span<const TerrainLayerControlSample> idSamples = config->GetLayerIDSamples();
    const std::span<const TerrainLayerControlSample> weightSamples = config->GetLayerWeightSamples();

    const float worldXScale = glm::length(glm::vec3(model[0]));
    const float worldZScale = glm::length(glm::vec3(model[2]));
    if (worldXScale <= 0.0f || worldZScale <= 0.0f)
        return;
    const float localRadiusX = brushRadius / worldXScale;
    const float localRadiusZ = brushRadius / worldZScale;
    const float centerU = localHit.x / size.x + 0.5f;
    const float centerV = localHit.z / size.y + 0.5f;
    const int minX = glm::clamp(
        static_cast<int>(std::floor((centerU - localRadiusX / size.x) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int maxX = glm::clamp(
        static_cast<int>(std::ceil((centerU + localRadiusX / size.x) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int minY = glm::clamp(
        static_cast<int>(std::floor((centerV - localRadiusZ / size.y) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );
    const int maxY = glm::clamp(
        static_cast<int>(std::ceil((centerV + localRadiusZ / size.y) * (resolution - 1))),
        0,
        static_cast<int>(resolution) - 1
    );

    std::vector<TerrainLayerControlValue> changes;
    changes.reserve(static_cast<size_t>(maxX - minX + 1) * (maxY - minY + 1));
    for (int y = minY; y <= maxY; ++y)
    {
        const float v = static_cast<float>(y) / static_cast<float>(resolution - 1);
        const float localZ = (v - 0.5f) * size.y;
        for (int x = minX; x <= maxX; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
            const float localX = (u - 0.5f) * size.x;
            const glm::vec3 worldOffset = model * glm::vec4(localX - localHit.x, 0.0f, localZ - localHit.z, 0.0f);
            const float radialDistance = glm::length(worldOffset);
            if (radialDistance > brushRadius)
                continue;

            const float influence = std::pow(1.0f - radialDistance / brushRadius, brushFalloff);
            if (influence <= 0.0f)
                continue;
            const uint32_t index = static_cast<uint32_t>(y) * resolution + static_cast<uint32_t>(x);
            auto working = layerStrokeWorking.find(index);
            if (working == layerStrokeWorking.end())
            {
                working = layerStrokeWorking.emplace(
                    index,
                    TerrainLayerPainting::CreateWorkingSample(idSamples[index], weightSamples[index])
                ).first;
            }
            const size_t selectedChannel = TerrainLayerPainting::PrepareSelectedLayer(
                working->second,
                static_cast<uint8_t>(selectedLayerID)
            );
            TerrainLayerPainting::IntegrateSelectedLayer(
                working->second,
                selectedChannel,
                layerStrength,
                deltaTime,
                influence
            );
            const TerrainLayerPainting::QuantizedSample painted = TerrainLayerPainting::Quantize(working->second);
            if (painted.ids == idSamples[index] && painted.weights == weightSamples[index])
                continue;

            layerStrokeBefore.try_emplace(index, TerrainLayerControlValue{
                .index = index,
                .ids = idSamples[index],
                .weights = weightSamples[index],
            });
            changes.push_back({index, painted.ids, painted.weights});
        }
    }
    config->ApplyLayerControlValues(changes);
}

void TerrainPaintTool::FinishStroke()
{
    if (!isPainting)
    {
        strokeWorkingHeights.clear();
        layerStrokeWorking.clear();
        return;
    }
    isPainting = false;
    strokeWorkingHeights.clear();

    Terrain* terrain = targetTerrain.Get();
    TerrainConfig* config = terrain != nullptr ? terrain->GetTerrainConfig() : nullptr;
    if (config == nullptr)
    {
        strokeBefore.clear();
        layerStrokeBefore.clear();
        layerStrokeWorking.clear();
        return;
    }

    if (!layerStrokeWorking.empty() && config->HasValidLayerControlData())
    {
        std::vector<TerrainLayerControlValue> canonicalChanges;
        for (const auto& [index, working] : layerStrokeWorking)
        {
            const TerrainLayerPainting::QuantizedSample canonical = TerrainLayerPainting::Quantize(working, true);
            const std::span<const TerrainLayerControlSample> ids = config->GetLayerIDSamples();
            const std::span<const TerrainLayerControlSample> weights = config->GetLayerWeightSamples();
            if (canonical.ids == ids[index] && canonical.weights == weights[index])
                continue;
            layerStrokeBefore.try_emplace(index, TerrainLayerControlValue{index, ids[index], weights[index]});
            canonicalChanges.push_back({index, canonical.ids, canonical.weights});
        }
        config->ApplyLayerControlValues(canonicalChanges);
    }

    if (!layerStrokeBefore.empty() && config->HasValidLayerControlData())
    {
        const std::span<const TerrainLayerControlSample> ids = config->GetLayerIDSamples();
        const std::span<const TerrainLayerControlSample> weights = config->GetLayerWeightSamples();
        std::vector<uint32_t> indices;
        indices.reserve(layerStrokeBefore.size());
        for (const auto& [index, value] : layerStrokeBefore)
        {
            if (ids[index] != value.ids || weights[index] != value.weights)
                indices.push_back(index);
        }
        std::sort(indices.begin(), indices.end());

        std::vector<TerrainLayerControlValue> before;
        std::vector<TerrainLayerControlValue> after;
        before.reserve(indices.size());
        after.reserve(indices.size());
        for (uint32_t index : indices)
        {
            before.push_back(layerStrokeBefore[index]);
            after.push_back({index, ids[index], weights[index]});
        }
        if (!before.empty())
        {
            EditorState::GetUndoManager().Execute(
                std::make_unique<TerrainLayerStrokeCommand>(config, std::move(before), std::move(after))
            );
        }
    }
    layerStrokeBefore.clear();
    layerStrokeWorking.clear();

    if (!strokeBefore.empty())
    {
        const std::span<const uint16_t> samples = config->GetHeightSamples();
        std::vector<uint32_t> indices;
        indices.reserve(strokeBefore.size());
        for (const auto& [index, value] : strokeBefore)
        {
            if (samples[index] != value)
                indices.push_back(index);
        }
        std::sort(indices.begin(), indices.end());

        std::vector<TerrainHeightValue> before;
        std::vector<TerrainHeightValue> after;
        before.reserve(indices.size());
        after.reserve(indices.size());
        for (uint32_t index : indices)
        {
            before.push_back({index, strokeBefore[index]});
            after.push_back({index, samples[index]});
        }
        if (!before.empty())
        {
            EditorState::GetUndoManager().Execute(
                std::make_unique<TerrainHeightStrokeCommand>(config, std::move(before), std::move(after))
            );
        }
    }
    strokeBefore.clear();
}

void TerrainPaintTool::OnDraw(Gfx::CommandBuffer& cmd)
{
    if (hasHit && HasTarget())
        DrawBrush(cmd);
}

void TerrainPaintTool::DrawBrush(Gfx::CommandBuffer& cmd)
{
    Terrain* terrain = targetTerrain.Get();
    TerrainConfig* config = terrain->GetTerrainConfig();
    GameObject* owner = terrain->GetGameObject();
    const glm::mat4 model = owner->GetWorldMatrix();
    const glm::mat4 inverseModel = glm::inverse(model);
    const glm::vec3 axisX = glm::normalize(glm::vec3(model[0]));
    const glm::vec3 axisZ = glm::normalize(glm::vec3(model[2]));
    glm::vec4 color;
    if (brushType == TerrainPaintBrushType::Layer)
        color = CanPaintSelectedLayer() ? glm::vec4(0.7f, 0.2f, 1.0f, 1.0f)
                                        : glm::vec4(1.0f, 0.15f, 0.15f, 1.0f);
    else if (brushType == TerrainPaintBrushType::Smooth)
        color = glm::vec4(0.15f, 0.65f, 1.0f, 1.0f);
    else if (ImGui::GetIO().KeyShift)
        color = glm::vec4(1.0f, 0.25f, 0.1f, 1.0f);
    else
        color = glm::vec4(0.15f, 1.0f, 0.2f, 1.0f);
    constexpr int SegmentCount = 48;
    std::vector<glm::vec3> points(SegmentCount);
    for (int i = 0; i < SegmentCount; ++i)
    {
        const float angle = static_cast<float>(i) / SegmentCount * 6.28318530718f;
        const glm::vec3 planarPoint = lastHitPoint +
                                      (axisX * std::cos(angle) + axisZ * std::sin(angle)) * brushRadius;
        glm::vec3 localPoint = inverseModel * glm::vec4(planarPoint, 1.0f);
        const float2 size = config->GetSize();
        const float2 uv(localPoint.x / size.x + 0.5f, localPoint.z / size.y + 0.5f);
        localPoint.y = config->SampleHeight(uv) + 0.03f;
        points[i] = model * glm::vec4(localPoint, 1.0f);
    }

    std::vector<TerrainBrushLine> lines;
    lines.reserve(SegmentCount);
    for (int i = 0; i < SegmentCount; ++i)
        lines.push_back({glm::vec4(points[i], 1.0f), glm::vec4(points[(i + 1) % SegmentCount], 1.0f), color});

    auto lineBuffer = cmd.AllocateBuffer(
        sizeof(TerrainBrushLine) * lines.size(),
        Gfx::TemporaryBufferUsage::Storage,
        alignof(TerrainBrushLine)
    );
    cmd.UploadData(lineBuffer, lines.data(), sizeof(TerrainBrushLine) * lines.size());
    Shader& lineShader = EngineInternalResources::GetLineShader();
    Gfx::ShaderProgram* lineProgram = lineShader.GetShaderProgram();
    cmd.BindResource(
        lineShader.GetSet(Gfx::DescriptorSetSemantics::Material),
        std::vector<Gfx::DynamicBinding>{Gfx::DynamicBinding("lineData", lineBuffer)}
    );
    cmd.BindShaderProgram(lineProgram, lineProgram->GetDefaultPipelineConfig());
    cmd.Draw(static_cast<uint32_t>(lines.size() * 2), 1, 0, 0);
}

void TerrainPaintTool::OnActivate()
{
    isPainting = false;
    strokeBefore.clear();
    strokeWorkingHeights.clear();
    layerStrokeBefore.clear();
    layerStrokeWorking.clear();
}

void TerrainPaintTool::OnDeactivate()
{
    FinishStroke();
    hasHit = false;
}
} // namespace Editor
