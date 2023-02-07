#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine
{
#define SER_MEMS()                                                                                                     \
    SERIALIZE_MEMBER(mesh);                                                                                            \
    SERIALIZE_MEMBER(materials);

MeshRenderer::MeshRenderer(GameObject* parent, Mesh2* mesh, Material* material)
    : Component("MeshRenderer", parent), mesh(mesh), materials({material})
{
    SER_MEMS();
    TryCreateObjectShaderResource();
}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) { SER_MEMS(); }

MeshRenderer::MeshRenderer() : Component("MeshRenderer", nullptr), mesh(nullptr), materials() { SER_MEMS(); };

void MeshRenderer::SetMesh(Mesh2* mesh)
{
    this->mesh = mesh;
    this->materials.resize(mesh->submeshes.size());
}

void MeshRenderer::SetMaterials(std::span<Material*> materials)
{
    this->materials = std::vector<Material*>(materials.begin(), materials.end());
    TryCreateObjectShaderResource();
}

Mesh2* MeshRenderer::GetMesh() { return mesh; }

const std::vector<Material*>& MeshRenderer::GetMaterials() { return materials; }

void MeshRenderer::TryCreateObjectShaderResource()
{
    // for (auto material : materials)
    //{
    //     if (material != nullptr && material->GetShader() != nullptr)
    //     {
    //         objectShaderResource =
    //             Gfx::GfxDriver::Instance()->CreateShaderResource(material->GetShader()->GetShaderProgram(),
    //                                                              Gfx::ShaderResourceFrequency::Object);
    //     }
    // }
}
} // namespace Engine
