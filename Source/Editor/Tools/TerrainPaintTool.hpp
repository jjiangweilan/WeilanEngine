#pragma once

#include "Editor/SceneEditorTool.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/Object/Component/Terrain.hpp"
#include <unordered_map>

namespace Editor
{
enum class TerrainPaintBrushType
{
    Height,
    Smooth
};

class TerrainPaintTool final : public SceneEditorTool
{
public:
    void SetTargetTerrain(Terrain* terrain);
    Terrain* GetTargetTerrain() const { return targetTerrain.Get(); }
    bool HasTarget() const;
    std::string GetTargetName() const;

    bool Tick(const SceneEditorToolContext& ctx) override;
    void OnDraw(Gfx::CommandBuffer& cmd) override;
    void OnActivate() override;
    void OnDeactivate() override;

    TerrainPaintBrushType brushType = TerrainPaintBrushType::Height;
    float brushRadius = 5.0f;
    float brushStrength = 8.0f;
    float smoothStrength = 6.0f;
    float brushFalloff = 2.0f;

private:
    ObjPtr<Terrain> targetTerrain;
    bool isPainting = false;
    bool hasHit = false;
    float3 lastHitPoint = float3(0.0f);
    float3 lastHitNormal = float3(0.0f, 1.0f, 0.0f);
    std::unordered_map<uint32_t, uint16_t> strokeBefore;
    std::unordered_map<uint32_t, float> strokeWorkingHeights;

    void ApplyBrush(const float3& worldPoint, float direction, float deltaTime);
    void FinishStroke();
    void DrawBrush(Gfx::CommandBuffer& cmd);
};
} // namespace Editor
