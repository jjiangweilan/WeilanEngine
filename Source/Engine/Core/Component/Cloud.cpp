#include "Cloud.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(Cloud, "D659B514-6D77-498B-88DB-F20FC0F62B10");
Cloud::Cloud() : Component(nullptr), mesh(nullptr){};

Cloud::Cloud(GameObject* owner) : Component(owner), mesh(nullptr){};

void Cloud::SetMesh(Mesh* mesh)
{
    this->mesh = mesh;
}

Mesh* Cloud::GetMesh()
{
    return mesh;
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
