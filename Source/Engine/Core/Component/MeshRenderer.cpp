#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(MeshRenderer, "00412ED6-89D3-4DD3-9D56-754820250E78");
MeshRenderer::MeshRenderer(GameObject* parent, Mesh* mesh, Material* material)
    : Component(parent), meshes(), materials({material})
{}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) {}

MeshRenderer::MeshRenderer() : Component(nullptr), meshes(), materials() {};

void MeshRenderer::SetMesh(Mesh* mesh)
{
    Mesh* meshes[] = {mesh};
    SetMeshes(meshes);
}

void MeshRenderer::SetMeshes(std::span<Mesh*> meshes)
{
    this->meshes.clear();
    this->meshes = std::vector(meshes.begin(), meshes.end());
    this->materials.resize(meshes.size());
    UpdateAABB();
}

void MeshRenderer::UpdateAABB()
{
    glm::vec3 min =
        {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    glm::vec3 max =
        {std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};

    for (auto& mesh : meshes)
    {
        auto& aabb = mesh->GetAABB();
        min.x = glm::min(min.x, aabb.min.x);
        min.y = glm::min(min.y, aabb.min.y);
        min.z = glm::min(min.z, aabb.min.z);
        max.x = glm::max(max.x, aabb.max.x);
        max.y = glm::max(max.y, aabb.max.y);
        max.z = glm::max(max.z, aabb.max.z);
    }

    aabb = {min, max};
}

void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials = std::vector<Material*>(materials.begin(), materials.end());
}

Mesh* MeshRenderer::GetMesh()
{
    return meshes.empty() ? nullptr : meshes[0];
}

std::span<Mesh*> MeshRenderer::GetMeshes()
{
    return meshes;
}

const std::vector<Material*>& MeshRenderer::GetMaterials()
{
    return materials;
}

void MeshRenderer::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("meshes", meshes);
    s->Serialize("materials", materials);
    s->Serialize("aabbMin", aabb.min);
    s->Serialize("aabbMax", aabb.max);
}

void MeshRenderer::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("meshes", meshes);
    s->Deserialize("materials", materials);
    s->Deserialize("aabbMin", aabb.min);
    s->Deserialize("aabbMax", aabb.max);
}

std::unique_ptr<Component> MeshRenderer::Clone(GameObject& owner)
{
    auto clone = std::make_unique<MeshRenderer>(&owner);

    clone->meshes = meshes;
    clone->materials = materials;

    if (IsEnabled())
    {
        clone->enabled = true;
    }

    return clone;
}

const std::string& MeshRenderer::GetName()
{
    static std::string name = "MeshRenderer";
    return name;
}

void MeshRenderer::AddToRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->AddRenderer(*this);
    }
}
void MeshRenderer::RemoveFromRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->RemoveRenderer(*this);
    }
}

void MeshRenderer::EnableImple()
{
    AddToRenderingScene();
}
void MeshRenderer::DisableImple()
{
    RemoveFromRenderingScene();
}

AABB MeshRenderer::GetAABB()
{
    auto model = GetGameObject()->GetWorldMatrix();
    AABB aabb = this->aabb;
    aabb.Transform(glm::mat3(model), model[3]);
    return aabb;
}

void MeshRenderer::Tick() {
    if(animation.HasAnimation())
    {
    }
}

void MeshRenderer::BindSkeletonAnimation(Animation anim)
{

}
