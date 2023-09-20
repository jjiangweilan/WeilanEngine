#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine
{
DEFINE_OBJECT(MeshRenderer, "00412ED6-89D3-4DD3-9D56-754820250E78");
MeshRenderer::MeshRenderer(GameObject* parent, Mesh2* mesh, Material* material)
    : Component("MeshRenderer", parent), mesh(mesh), materials({material})
{}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) {}

MeshRenderer::MeshRenderer() : Component("MeshRenderer", nullptr), mesh(nullptr), materials(){};

void MeshRenderer::SetMesh(Mesh2* mesh)
{
    this->mesh = mesh;
    this->materials.resize(mesh->submeshes.size());
}

void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials = std::vector<Material*>(materials.begin(), materials.end());
}

Mesh2* MeshRenderer::GetMesh()
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

} // namespace Engine
