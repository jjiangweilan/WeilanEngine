#include "Terrain.hpp"
#include "Core/Scene/Scene.hpp"

DEFINE_OBJECT(Terrain, "F9F80900-D99F-45F4-A41D-B3414B3CCB0F");
Terrain::Terrain() : Component(nullptr) {}
Terrain::Terrain(GameObject* owner) : Component(owner) {}

void Terrain::Serialize(Serializer* s) const
{
    Component::Serialize(s);
}
void Terrain::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
}
const std::string& Terrain::GetName()
{
    static std::string name = "Terrain";
    return name;
}
std::unique_ptr<Component> Terrain::Clone(GameObject& owner)
{
    return std::make_unique<Terrain>(&owner);
}

void Terrain::EnableImple()
{
    if (auto scene = GetScene())
    {
        scene->GetRenderingScene().SetTerrain(*this);
    }
}

void Terrain::DisableImple()
{
    if (auto scene = GetScene())
    {
        scene->GetRenderingScene().RemoveTerrain(*this);
    }
}
