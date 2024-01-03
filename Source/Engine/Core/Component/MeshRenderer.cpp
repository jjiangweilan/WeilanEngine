#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(MeshRenderer, "00412ED6-89D3-4DD3-9D56-754820250E78");
MeshRenderer::MeshRenderer(GameObject* parent, Mesh* mesh, Material* material)
    : Component(parent), mesh(mesh), materials({material})
{}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) {}

MeshRenderer::MeshRenderer() : Component(nullptr), mesh(nullptr), materials(){};

void MeshRenderer::SetMesh(Mesh* mesh)
{
    this->mesh = mesh;
    this->materials.resize(mesh->GetSubmeshes().size());
}

void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials = std::vector<Material*>(materials.begin(), materials.end());
}

Mesh* MeshRenderer::GetMesh()
{
    return mesh;
}

const std::vector<Material*>& MeshRenderer::GetMaterials()
{
    return materials;
}

void MeshRenderer::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("mesh", mesh);
    s->Serialize("materials", materials);
}
void MeshRenderer::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("mesh", mesh);
    s->Deserialize("materials", materials);
}

std::unique_ptr<Component> MeshRenderer::Clone(GameObject& owner)
{
    auto clone = std::make_unique<MeshRenderer>(&owner);

    clone->mesh = mesh;
    clone->materials = materials;
    clone->aabb = aabb;

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
