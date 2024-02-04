#include "Cloud.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Cloud, "D659B514-6D77-498B-88DB-F20FC0F62B10");
Cloud::Cloud(GameObject* parent) : Cloud(parent, nullptr, nullptr) {}

Cloud::Cloud() : Component(nullptr), mesh(nullptr), materials(){};

void Cloud::SetMesh(Mesh* mesh)
{
    this->mesh = mesh;
    this->materials.resize(mesh->GetSubmeshes().size());
}

void Cloud::SetMaterials(std::span<Material*> materials)
{
    this->materials = std::vector<Material*>(materials.begin(), materials.end());
}

Mesh* Cloud::GetMesh()
{
    return mesh;
}

const std::vector<Material*>& Cloud::GetMaterials()
{
    return materials;
}

void Cloud::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("mesh", mesh);
}
void Cloud::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("mesh", mesh);
}

std::unique_ptr<Component> Cloud::Clone(GameObject& owner)
{
    auto clone = std::make_unique<Cloud>(&owner);

    clone->mesh = mesh;
    clone->aabb = aabb;

    return clone;
}

const std::string& Cloud::GetName()
{
    static std::string name = "Cloud";
    return name;
}

void Cloud::AddToRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->AddRenderer(*this);
    }
}
void Cloud::RemoveFromRenderingScene()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto renderingScene = &scene->GetRenderingScene();
        renderingScene->RemoveRenderer(*this);
    }
}

void Cloud::EnableImple()
{
    AddToRenderingScene();
}
void Cloud::DisableImple()
{
    RemoveFromRenderingScene();
}
