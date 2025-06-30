#include "ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"

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
    auto proj = Math::GetProjectionMatrix(90, 1, 0.1, 1000);
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
        auto view = glm::translate(glm::mat4(1.0f), position) *
                    glm::mat4_cast(glm::rotate(glm::quat(1, 0, 0, 0), 0.0f, faces[i]));

        frustums[i] = proj * glm::inverse(view);
    }
}

Gfx::Image* ReflectionProbe::GetCubemap()
{
    return cubemap.get();
}
