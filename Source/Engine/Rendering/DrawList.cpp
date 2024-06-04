#include "DrawList.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/GameObject.hpp"
namespace Rendering
{
void swap(SceneObjectDrawData&& a, SceneObjectDrawData&& b)
{
    SceneObjectDrawData c(std::move(a));
    a = std::move(b);
    b = std::move(c);
}

void DrawList::Add(MeshRenderer& meshRenderer)
{
    auto mesh = meshRenderer.GetMesh();
    if (mesh == nullptr)
        return;

    auto& submeshes = mesh->GetSubmeshes();
    auto& materials = meshRenderer.GetMaterials();

    if (!meshRenderer.IsMultipassEnabled())
    {
        for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
        {
            auto material = i < materials.size() ? materials[i] : nullptr;
            auto submesh = i < submeshes.size() ? &submeshes[i] : nullptr;
            auto shader = material ? material->GetShader() : nullptr;

            if (submesh != nullptr && material != nullptr && shader != nullptr)
            {
                uint32_t indexCount = submesh->GetIndexCount();

                SceneObjectDrawData drawData;
                drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                for (auto& binding : submesh->GetBindings())
                {
                    drawData.vertexBufferBinding.push_back({submesh->GetVertexBuffer(), binding.byteOffset});
                }
                drawData.indexBuffer = submesh->GetIndexBuffer();
                drawData.indexBufferType = submesh->GetIndexBufferType();

                material->UploadDataToGPU();
                drawData.shaderResource = material->GetShaderResource();
                drawData.shader = (Shader*)shader;
                drawData.shaderConfig = &material->GetShaderConfig();
                auto modelMatrix = meshRenderer.GetGameObject()->GetModelMatrix();
                drawData.pushConstant = modelMatrix;
                drawData.indexCount = indexCount;
                drawData.material = material;

                push_back(std::move(drawData));
            }
        }
    }
    else
    {
        for (int i = 0; i < materials.size(); ++i)
        {
            auto material = materials[i];
            if (material == nullptr)
                continue;

            auto shader = material ? material->GetShaderProgram() : nullptr;
            material->UploadDataToGPU();
            for (auto& submesh : submeshes)
            {
                if (material != nullptr && shader != nullptr)
                {
                    // material->SetMatrix("Transform", "model",
                    // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                    uint32_t indexCount = submesh.GetIndexCount();

                    SceneObjectDrawData drawData;
                    drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                    for (auto& binding : submesh.GetBindings())
                    {
                        drawData.vertexBufferBinding.push_back({submesh.GetVertexBuffer(), binding.byteOffset});
                    }
                    drawData.indexBuffer = submesh.GetIndexBuffer();
                    drawData.indexBufferType = submesh.GetIndexBufferType();

                    drawData.shaderResource = material->GetShaderResource();
                    drawData.shader = (Shader*)shader;
                    drawData.shaderConfig = &material->GetShaderConfig();
                    auto modelMatrix = meshRenderer.GetGameObject()->GetModelMatrix();
                    drawData.pushConstant = modelMatrix;
                    drawData.indexCount = indexCount;

                    push_back(std::move(drawData));
                }
            }
        }
    }
}
}