#include "MeshRenderer.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine
{
#define SER_MEMS() \
        SERIALIZE_MEMBER(mesh); \
        SERIALIZE_MEMBER(material);

    MeshRenderer::MeshRenderer(GameObject* parent, Mesh* mesh, Material* material) : Component("MeshRenderer", parent), mesh(mesh), material(material)
    {
        SER_MEMS();
        TryCreateObjectShaderResource();
    }

    MeshRenderer::MeshRenderer(GameObject* parent)  : MeshRenderer(parent, nullptr, nullptr)
    {
        SER_MEMS();
    }

    MeshRenderer::MeshRenderer() : Component("MeshRenderer", nullptr), mesh(nullptr), material(nullptr)
    {
        SER_MEMS();
    };

    void MeshRenderer::SetMesh(Mesh* mesh)
    {
        this->mesh = mesh;
    }

    void MeshRenderer::SetMaterial(Material* material)
    {
        this->material = material;
        TryCreateObjectShaderResource();
    }

    Mesh* MeshRenderer::GetMesh()
    {
        return mesh;
    }

    Material* MeshRenderer::GetMaterial()
    {
        return material;
    }

    void MeshRenderer::Tick()
    {
        if (material)
        {
            glm::mat4 model = gameObject->GetTransform()->GetModelMatrix();
            material->SetMatrix("Transform", "model", model);
        }
    }

    void MeshRenderer::TryCreateObjectShaderResource()
    {
        if (material != nullptr && material->GetShader() != nullptr)
        {
            objectShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(material->GetShader()->GetShaderProgram(), Gfx::ShaderResourceFrequency::Object);
        }
    }
}
