#pragma once

#include "Editor/SceneEditorTool.hpp"
#include "Editor/Tools/TerrainLayerPainting.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/Object/Component/Terrain.hpp"
#include <unordered_map>

namespace Editor
{
enum class TerrainPaintBrushType
{
    Height,
    Smooth,
    Layer
};

class TerrainPaintTool final : public SceneEditorTool
{
public:
    void SetTargetTerrain(Terrain* terrain);
    Terrain* GetTargetTerrain() const { return targetTerrain.Get(); }
    bool HasTarget() const;
    std::string GetTargetName() const;
    void SetBrushType(TerrainPaintBrushType value);
    TerrainPaintBrushType GetBrushType() const { return brushType; }
    void SetSelectedLayerID(uint32_t value);
    uint32_t GetSelectedLayerID() const { return selectedLayerID; }
    bool CanPaintSelectedLayer() const;

    bool Tick(const SceneEditorToolContext& ctx) override;
    void OnDraw(Gfx::CommandBuffer& cmd) override;
    void OnActivate() override;
    void OnDeactivate() override;

    float brushRadius = 5.0f;
    float brushStrength = 8.0f;
    float smoothStrength = 6.0f;
    float layerStrength = 6.0f;
    float brushFalloff = 2.0f;

private:
    ObjPtr<Terrain> targetTerrain;
    TerrainPaintBrushType brushType = TerrainPaintBrushType::Height;
    uint32_t selectedLayerID = TerrainConfig::InvalidTerrainLayerID;
    bool isPainting = false;
    bool hasHit = false;
    float3 lastHitPoint = float3(0.0f);
    float3 lastHitNormal = float3(0.0f, 1.0f, 0.0f);
    std::unordered_map<uint32_t, uint16_t> strokeBefore;
    std::unordered_map<uint32_t, float> strokeWorkingHeights;
    std::unordered_map<uint32_t, TerrainLayerControlValue> layerStrokeBefore;
    std::unordered_map<uint32_t, TerrainLayerPainting::WorkingSample> layerStrokeWorking;

    void ApplyBrush(const float3& worldPoint, float direction, float deltaTime);
    void ApplyLayerBrush(const float3& worldPoint, float deltaTime);
    void FinishStroke();
    void DrawBrush(Gfx::CommandBuffer& cmd);
};
} // namespace Editor
