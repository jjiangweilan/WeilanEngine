#pragma once
#include "Editor/SceneEditorTool.hpp"
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include <random>

struct SurfaceHit;

namespace Editor
{
class GrassSurfacePaintTool : public SceneEditorTool
{
public:
    GrassSurfacePaintTool() = default;

    bool Tick(const SceneEditorToolContext& ctx) override;
    void OnDraw(Gfx::CommandBuffer& cmd) override;
    void OnActivate() override;
    void OnDeactivate() override;

    // Brush settings (public so window can read/write them)
    float brushRadius = 1.0f;
    int density = 1;
    int meshIndex = 0;
    bool eraseMode = false;

    int GetPatchCount() const;
    std::string GetTargetName() const;
    GrassSurface* GetTargetGrassSurface() const { return lastHitGrassSurface; }
    void ClampMeshIndex();

private:
    struct PendingSample {
        glm::vec3 position;
    };

    bool isPainting = false;
    bool wasMouseDown = false;
    GameObject* lastHitObject = nullptr;
    GrassSurface* lastHitGrassSurface = nullptr;
    glm::vec3 lastHitPoint;
    glm::vec3 lastHitNormal;

    std::mt19937 rng{std::random_device{}()};
    std::vector<PendingSample> currentStrokeSamples;

    void PaintStroke(const SurfaceHit& hit);
    void EraseStroke(const SurfaceHit& hit);
    void DrawWireCircle(Gfx::CommandBuffer& cmd, const glm::vec3& center, const glm::vec3& normal,
                        float radius, const glm::vec4& color);
    void BuildONB(const glm::vec3& normal, glm::vec3& outTangent, glm::vec3& outBitangent);
    GrassSurface* FindGrassSurfaceInChain(GameObject* go);
    bool GroundPointOnObject(const glm::vec3& point, const glm::vec3& normal, GameObject* obj, glm::vec3& outGrounded);
};
} // namespace Editor
