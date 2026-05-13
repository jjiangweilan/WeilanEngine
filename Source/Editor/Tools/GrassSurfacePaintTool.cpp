#include "GrassSurfacePaintTool.hpp"
#include "Editor/EditorContext.hpp"
#include "Editor/PickObjectFromGameView.hpp"
#include "Editor/SceneEditor.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/ThirdParty/imgui/ImGuizmo.h"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <cmath>

namespace Editor
{

GrassSurface* GrassSurfacePaintTool::FindGrassSurfaceInChain(GameObject* go)
{
    while (go)
    {
        if (auto gs = go->GetComponent<GrassSurface>())
            return gs;
        go = go->GetParent();
    }
    return nullptr;
}

bool GrassSurfacePaintTool::Tick(const SceneEditorToolContext& ctx)
{
    if (!ctx.sceneViewHovered)
    {
        lastHitObject = nullptr;
        lastHitGrassSurface = nullptr;
        return false;
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing() || (ctx.gizmoManager && ctx.gizmoManager->AnyGizmoActive()))
    {
        currentStrokeSamples.clear();
        return false;
    }

    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    auto scene = SceneManager::GetActiveScene();
    if (!scene)
        return false;

    SurfaceHit hit = RaycastSceneSurface(ctx.worldRay, *scene);
    lastHitObject = hit.go;
    GrassSurface* prevGrassSurface = lastHitGrassSurface;
    lastHitGrassSurface = hit.go ? FindGrassSurfaceInChain(hit.go) : nullptr;
    lastHitPoint = hit.point;
    lastHitNormal = hit.normal;

    if (lastHitGrassSurface != prevGrassSurface)
    {
        ClampMeshIndex();
    }

    if (mouseDown || mouseClicked)
    {
        if (mouseClicked && lastHitGrassSurface)
        {
            isPainting = true;
            currentStrokeSamples.clear();
        }

        if (isPainting && lastHitGrassSurface)
        {
            if (eraseMode)
                EraseStroke(hit);
            else
                PaintStroke(hit);
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        isPainting = false;
        currentStrokeSamples.clear();
    }

    return true;
}

void GrassSurfacePaintTool::PaintStroke(const SurfaceHit& hit)
{
    GrassSurface* gs = lastHitGrassSurface;
    if (!gs)
        return;
    if (gs->grassPatchGroup.patchMeshes.empty())
        return;

    glm::vec3 tangent, bitangent;
    BuildONB(hit.normal, tangent, bitangent);

    float minSpacing = spacing;
    std::uniform_real_distribution<float> distRadius(0.0f, 1.0f);
    std::uniform_real_distribution<float> distAngle(0.0f, 6.28318530718f);

    int placed = 0;
    int maxAttempts = density * 10;
    for (int attempt = 0; attempt < maxAttempts && placed < density; ++attempt)
    {
        float r = brushRadius * glm::sqrt(distRadius(rng));
        float theta = distAngle(rng);

        glm::vec3 offset = tangent * (r * glm::cos(theta)) + bitangent * (r * glm::sin(theta));
        glm::vec3 candidate = hit.point + offset;

        glm::vec3 grounded;
        glm::vec3 groundedNormal;
        if (!GroundPointOnObject(candidate, hit.normal, hit.go, grounded, groundedNormal))
            continue;

        if (matchCenterNormal)
        {
            float dotProd = glm::dot(glm::normalize(hit.normal), glm::normalize(groundedNormal));
            float threshold = glm::cos(glm::radians(normalToleranceAngle));
            if (dotProd < threshold)
                continue;
        }

        // Reject if too close to existing patches
        bool tooClose = false;
        for (const auto& patch : gs->grassPatchGroup.patches)
        {
            if (glm::distance(grounded, patch.position) < minSpacing * 0.5f)
            {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;

        // Reject if too close to other samples in this stroke
        for (const auto& s : currentStrokeSamples)
        {
            if (glm::distance(grounded, s.position) < minSpacing)
            {
                tooClose = true;
                break;
            }
        }
        if (tooClose)
            continue;

        std::uniform_int_distribution<int> distMesh(0, static_cast<int>(gs->grassPatchGroup.patchMeshes.size()) - 1);
        int idx = distMesh(rng);

        gs->grassPatchGroup.patches.push_back({grounded, idx});
        currentStrokeSamples.push_back({grounded});
        placed++;
    }
}

void GrassSurfacePaintTool::EraseStroke(const SurfaceHit& hit)
{
    GrassSurface* gs = lastHitGrassSurface;
    if (!gs)
        return;

    auto& patches = gs->grassPatchGroup.patches;
    patches.erase(
        std::remove_if(
            patches.begin(),
            patches.end(),
            [&](const GrassPatch& p)
            { return glm::distance(p.position, hit.point) < brushRadius; }
        ),
        patches.end()
    );
}

bool GrassSurfacePaintTool::GroundPointOnObject(const glm::vec3& point, const glm::vec3& normal, GameObject* obj, glm::vec3& outGrounded, glm::vec3& outGroundedNormal)
{
    Ray groundRay(point + normal * 100.0f, -normal);
    float distance;
    glm::vec3 hitPoint, hitNormal;
    if (PickGameObjectFromScene::IsRayObjectIntersect(groundRay.origin, groundRay.direction, obj, distance, hitPoint, hitNormal) && distance > 0.0f && distance < 200.0f)
    {
        outGrounded = hitPoint;
        outGroundedNormal = hitNormal;
        return true;
    }
    // Fallback: just use the candidate point
    outGrounded = point;
    outGroundedNormal = normal;
    return true;
}

void GrassSurfacePaintTool::BuildONB(const glm::vec3& normal, glm::vec3& outTangent, glm::vec3& outBitangent)
{
    glm::vec3 up = glm::abs(normal.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    outTangent = glm::normalize(glm::cross(up, normal));
    outBitangent = glm::normalize(glm::cross(normal, outTangent));
}

void GrassSurfacePaintTool::OnDraw(Gfx::CommandBuffer& cmd)
{
    if (!lastHitObject)
        return; 

    bool paintable = lastHitGrassSurface != nullptr;
    glm::vec4 color = paintable ? glm::vec4(0, 1, 0, 1) : glm::vec4(1, 0, 0, 1);
    DrawWireCircle(cmd, lastHitPoint, lastHitNormal, brushRadius, color);
}

void GrassSurfacePaintTool::DrawWireCircle(Gfx::CommandBuffer& cmd, const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec4& color)
{
    glm::vec3 tangent, bitangent;
    BuildONB(normal, tangent, bitangent);

    const int segments = 32;
    Gfx::ShaderProgram* lineShaderProgram = EngineInternalResources::GetLineShader().GetShaderProgram();

    for (int i = 0; i < segments; ++i)
    {
        float a0 = (i / (float)segments) * 2.0f * 3.14159265359f;
        float a1 = ((i + 1) / (float)segments) * 2.0f * 3.14159265359f;

        glm::vec3 p0 = center + (tangent * glm::cos(a0) + bitangent * glm::sin(a0)) * radius;
        glm::vec3 p1 = center + (tangent * glm::cos(a1) + bitangent * glm::sin(a1)) * radius;

        struct
        {
            glm::vec4 fromPos;
            glm::vec4 toPos;
            glm::vec4 color;
        } data;
        data.fromPos = glm::vec4(p0, 1.0f);
        data.toPos = glm::vec4(p1, 1.0f);
        data.color = color;

        cmd.SetPushConstant(lineShaderProgram, (void*)&data);
        cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultShaderConfig());
        cmd.Draw(2, 1, 0, 0);
    }
}

void GrassSurfacePaintTool::OnActivate()
{
    isPainting = false;
    currentStrokeSamples.clear();
}

void GrassSurfacePaintTool::OnDeactivate()
{
    isPainting = false;
    currentStrokeSamples.clear();
}

int GrassSurfacePaintTool::GetPatchCount() const
{
    if (lastHitGrassSurface)
        return (int)lastHitGrassSurface->grassPatchGroup.patches.size();
    return 0;
}

void GrassSurfacePaintTool::ClampMeshIndex()
{
    if (lastHitGrassSurface && !lastHitGrassSurface->grassPatchGroup.patchMeshes.empty())
    {
        int maxIndex = (int)lastHitGrassSurface->grassPatchGroup.patchMeshes.size() - 1;
        if (meshIndex > maxIndex)
            meshIndex = maxIndex;
    }
    else
    {
        meshIndex = 0;
    }
}

std::string GrassSurfacePaintTool::GetTargetName() const
{
    if (lastHitObject)
        return lastHitObject->GetName();
    return "None";
}

} // namespace Editor
