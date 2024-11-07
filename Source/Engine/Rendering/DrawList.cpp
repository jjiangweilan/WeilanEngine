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
    auto meshes = meshRenderer.GetMeshes();
    if (meshes.empty())
        return;

    auto& materials = meshRenderer.GetMaterials();

    if (!meshRenderer.IsMultipassEnabled())
    {
        for (int i = 0, mi = 0; i < meshes.size() || mi < materials.size(); ++i)
        {
            auto mesh = i < meshes.size() ? meshes[i] : nullptr;

            if (mesh != nullptr)
            {
                auto material = mi < materials.size() ? materials[mi] : nullptr;
                mi++;
                auto shader = material ? material->GetShader() : nullptr;
                if (material != nullptr && shader != nullptr)
                {
                    for (auto& submesh : mesh->GetSubmeshes())
                    {
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
                        auto modelMatrix = meshRenderer.GetGameObject()->GetWorldMatrix();
                        drawData.pushConstant = modelMatrix;
                        drawData.indexCount = indexCount;
                        drawData.material = material;
                        push_back(std::move(drawData));
                    }
                }
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
            for (auto mesh : meshes)
            {
                for (auto& submesh : mesh->GetSubmeshes())
                {
                    if (material != nullptr && shader != nullptr)
                    {
                        // material->SetMatrix("Transform", "model",
                        // meshRenderer->GetGameObject()->GetTransform()->GetWorldMatrix());
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
                        auto modelMatrix = meshRenderer.GetGameObject()->GetWorldMatrix();
                        drawData.pushConstant = modelMatrix;
                        drawData.indexCount = indexCount;

                        push_back(std::move(drawData));
                    }
                }
            }
        }
    }
}

void DrawList::Sort(const glm::vec3& cameraPos)
{
    std::sort(
        this->begin(),
        this->end(),
        [&cameraPos](const SceneObjectDrawData& left, const SceneObjectDrawData& right)
        {
            return glm::distance2(cameraPos, glm::vec3(left.pushConstant[3])) <
                   glm::distance2(cameraPos, glm::vec3(right.pushConstant[3]));
        }
    );

    // partition transparent object to the end
    auto transparentIter = std::stable_partition(
        this->begin(),
        this->end(),
        [](const SceneObjectDrawData& val)
        { return val.shaderConfig->color.blends.empty() ? true : !val.shaderConfig->color.blends[0].blendEnable; }
    );
    this->transparentIndex = std::distance(this->begin(), transparentIter);

    // partition opaque and alpha tested objects
    auto alphaTestIter = std::stable_partition(
        this->begin(),
        transparentIter,
        [](const SceneObjectDrawData& val)
        {
            static std::string alphaTest = "_AlphaTest";

            auto& features = val.material->GetCachedShaderProgramFeatureUsed();
            return std::find(features.begin(), features.end(), alphaTest) == features.end();
        }
    );
    this->alphaTestIndex = std::distance(this->begin(), alphaTestIter);
    this->opaqueIndex = 0;
}

void DrawList::Add(std::span<MeshRenderer*> meshRenderers)
{
    for (auto r : meshRenderers)
        if (r && r->IsEnabled())
        {
            this->Add(*r);
        }

    this->opaqueIndex = 0;
    this->alphaTestIndex = this->size();
    this->transparentIndex = this->size();
}
} // namespace Rendering
