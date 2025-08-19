#include "ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Editor/Gizmos/ScaleBoxGizmo.hpp"
#include "Rendering/RenderPipeline/RenderPipeline.hpp"

DEFINE_OBJECT(ReflectionProbe, "E93609AC-6D6C-4F13-9538-CD63E2D58567")

ReflectionProbe::ReflectionProbe() : Component(nullptr) {}
ReflectionProbe::ReflectionProbe(GameObject* go) : Component(go) {}
ReflectionProbe::~ReflectionProbe() {}
const std::string& ReflectionProbe::GetName()
{
    static std::string name = "ReflectionProbe";
    return name;
}

void ReflectionProbe::OnEnable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().AddRenderObject(*this);
    }

    UpdateFrustums(GetGameObject()->GetPosition());
}

void ReflectionProbe::OnDisable()
{
    auto scene = GetScene();
    if (scene)
    {
        scene->GetRenderingScene().RemoveRenderObject(*this);
    }
}

void ReflectionProbe::OnInit()
{
    Gfx::ImageDescription desc(256, 256, 1, Gfx::GfxFormat::R8G8B8A8_SRGB);
    desc.isCubemap = true;

    cubemap = GetGfxDriver()->CreateImage(desc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment);
}

void ReflectionProbe::TransformChanged()
{
    UpdateFrustums(GetGameObject()->GetPosition());
}

void ReflectionProbe::UpdateFrustums(float3 position)
{
    projectionMatrix = Math::GetProjectionMatrix(glm::radians(90.0f), 1, GetNear(), GetFar());
    float3 faces[] = {
        {1, 0, 0},  // +X
        {-1, 0, 0}, // -X
        {0, 1, 0},  // +Y
        {0, -1, 0}, // -Y
        {0, 0, 1},  // +Z
        {0, 0, -1}  // -Z
    };
    for (int i = 0; i < 6; ++i)
    {
        viewMatrices[i] =
            glm::lookAtRH(position, position + faces[i], (i != 2 && i != 3) ? float3(0, 1, 0) : float3(1, 0, 0));

        frustums[i] = projectionMatrix * viewMatrices[i];
    }
}

Gfx::Image* ReflectionProbe::GetCubemap()
{
    return cubemap.get();
}

const float4x4& ReflectionProbe::GetViewMatrix(int faceIdx)
{
    return viewMatrices[faceIdx];
}

const float4x4& ReflectionProbe::GetProjectionMatrix()
{
    return projectionMatrix;
}

float ReflectionProbe::GetNear()
{
    return near;
}

float ReflectionProbe::GetFar()
{
    return far;
}

float ReflectionProbe::GetProjectionTop()
{
    return near / projectionMatrix[0][0];
}

float ReflectionProbe::GetProjectionRight()
{
    return -near / projectionMatrix[1][1];
}

uint32_t ReflectionProbe::GetResolution()
{
    return resolution;
}

void ReflectionProbe::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, updateType);
    SERIALIZE(s, sourceType);
}

void ReflectionProbe::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    int iUpdateType;
    int iSourceType;
    DESERIALIZE(s, iSourceType);
    DESERIALIZE(s, iUpdateType);
    updateType = static_cast<ProbeType>(iUpdateType);
    sourceType = static_cast<SourceType>(iSourceType);
}

void ReflectionProbe::SetFar(float far)
{
    this->far = far;
    UpdateFrustums(gameObject->GetPosition());
}
void ReflectionProbe::SetNear(float near)
{
    this->near = near;
    UpdateFrustums(gameObject->GetPosition());
}

void ReflectionProbe::BakeStaticReflectionProbe()
{
    for (int face = 0; face < 6; face++)
    {
        std::unique_ptr<Rendering::RenderPipeline> renderPipeline = std::make_unique<Rendering::RenderPipeline>();
    }
}

void ReflectionProbe::OnDrawGizmos()
{
    Gizmos::DrawInteractiveBox(gizmoState, GetGameObject()->GetPosition(), extent);
}

void ReflectionProbe::OnDrawGizmos(GizmoContext& gizmoContext)
{
    gizmoContext.Draw<ScaleBoxGizmo>(scaleBoxGizmoHandle, gameObject->GetPosition(), gameObject->GetRotation(), extent);
}
