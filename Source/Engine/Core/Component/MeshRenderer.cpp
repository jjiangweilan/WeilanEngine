#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine
{
#define SER_MEMS()                                                                                                     \
    SERIALIZE_MEMBER(mesh);                                                                                            \
    SERIALIZE_MEMBER(material);

MeshRenderer::MeshRenderer(GameObject* parent, Mesh2* mesh, Material* material)
    : Component("MeshRenderer", parent), mesh(mesh), material(material)
{
    SER_MEMS();
    TryCreateObjectShaderResource();
}

MeshRenderer::MeshRenderer(GameObject* parent) : MeshRenderer(parent, nullptr, nullptr) { SER_MEMS(); }

MeshRenderer::MeshRenderer() : Component("MeshRenderer", nullptr), mesh(nullptr), material(nullptr) { SER_MEMS(); };

void MeshRenderer::SetMesh(Mesh2* mesh) { this->mesh = mesh; }

void MeshRenderer::SetMaterial(Material* material)
{
    this->material = material;
    TryCreateObjectShaderResource();
}

Mesh2* MeshRenderer::GetMesh() { return mesh; }

Material* MeshRenderer::GetMaterial() { return material; }

void MeshRenderer::TryCreateObjectShaderResource()
{
    if (material != nullptr && material->GetShader() != nullptr)
    {
        objectShaderResource =
            Gfx::GfxDriver::Instance()->CreateShaderResource(material->GetShader()->GetShaderProgram(),
                                                             Gfx::ShaderResourceFrequency::Object);
    }
}
} // namespace Engine
