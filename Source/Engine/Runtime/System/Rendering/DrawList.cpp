#include "DrawList.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"

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
        for (int i = 0, mi = 0; i < meshes.size() && mi < materials.size(); ++i)
        {
            auto mesh = i < meshes.size() ? meshes[i] : nullptr;

            if (mesh != nullptr)
            {
                for (auto& submesh : mesh->GetSubmeshes())
                {
                    auto material = mi < materials.size() ? materials[mi] : nullptr;
                    mi++;
                    auto shader = material ? material->GetShader() : nullptr;
                    if (material != nullptr && shader != nullptr)
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
                        drawData.materialSet = material->GetShader()->GetSet(Gfx::DescriptorSetSemantics::Material);
                        drawData.materialResource = material->GetShaderResource();
                        drawData.objectSet = material->GetSet(Gfx::DescriptorSetSemantics::Object);
                        drawData.objectResource = meshRenderer.GetObjectResource();
                        drawData.shader = shader.Get();
                        drawData.shaderConfig = &material->GetShaderConfig();
                        drawData.model = meshRenderer.GetGameObject()->GetWorldMatrix();
                        drawData.invTspModel = glm::inverse(glm::transpose(glm::float3x3(drawData.model)));
                        uint32_t indexOffset = submesh.GetGPUMeshIndexOffset();
                        uint32_t positionOffset = submesh.GetGPUMeshPositionOffset();
                        uint32_t attributeOffset = submesh.GetGPUMeshAttributeOffset();
                        drawData.invTspModel[3].x = std::bit_cast<float>(indexOffset);
                        drawData.invTspModel[3].y = std::bit_cast<float>(positionOffset);
                        drawData.invTspModel[3].z = std::bit_cast<float>(attributeOffset);
                        drawData.indexCount = indexCount;
                        drawData.material = material;
                        drawData.skinned = meshRenderer.IsSkinningEnabled();
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

            auto shader = material ? material->GetShader() : nullptr;
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
                        drawData.materialResource = material->GetShaderResource();
                        drawData.objectResource = meshRenderer.GetObjectResource();
                        drawData.shader = shader;
                        drawData.shaderConfig = &material->GetShaderConfig();
                        auto modelMatrix = meshRenderer.GetGameObject()->GetWorldMatrix();
                        drawData.model = modelMatrix;
                        uint32_t indexOffset = submesh.GetGPUMeshIndexOffset();
                        uint32_t positionOffset = submesh.GetGPUMeshPositionOffset();
                        uint32_t attributeOffset = submesh.GetGPUMeshAttributeOffset();
                        drawData.invTspModel = glm::inverse(glm::transpose(glm::float3x3(drawData.model)));
                        drawData.invTspModel[3].x = std::bit_cast<float>(indexOffset);
                        drawData.invTspModel[3].y = std::bit_cast<float>(positionOffset);
                        drawData.invTspModel[3].z = std::bit_cast<float>(attributeOffset);
                        drawData.indexCount = indexCount;

                        push_back(std::move(drawData));
                    }
                }
            }
        }
    }
}

void DrawList::Lock()
{
    sorted.clear();
    for (int i = 0; i < this->size(); ++i)
    {
        sorted.push_back(i);
    }
}

void DrawList::Sort(const glm::vec3& cameraPos)
{
    std::sort(
        sorted.begin(),
        sorted.end(),
        [&cameraPos, this](int left, int right)
        {
            return glm::distance2(cameraPos, glm::vec3(this->at(left).model[3])) <
                   glm::distance2(cameraPos, glm::vec3(this->at(right).model[3]));
        }
    );

    // partition transparent object to the end
    auto transparentIter = std::stable_partition(
        sorted.begin(),
        sorted.end(),
        [this](int val)
        {
            return (*this->at(val).shaderConfig)->color.blends.empty()
                       ? true
                       : !(*this->at(val).shaderConfig)->color.blends[0].blendEnable;
        }
    );
    this->transparentIndex = std::distance(sorted.begin(), transparentIter);

    // partition opaque and alpha tested objects
    auto alphaTestIter = std::stable_partition(
        sorted.begin(),
        transparentIter,
        [this](int val)
        {
            static std::string alphaTest = "_AlphaClip";

            auto features = this->at(val).material->GetCachedShaderProgramFeatureUsed();
            return std::find(features.begin(), features.end(), alphaTest) == features.end();
        }
    );
    this->alphaTestIndex = std::distance(sorted.begin(), alphaTestIter);
    this->opaqueIndex = 0;
}

void DrawList::Add(std::span<MeshRenderer*> meshRenderers)
{
    for (auto r : meshRenderers)
        if (r && r->IsActiveInScene() && !r->IsActiveGPUObject())
        {
            this->Add(*r);
        }

    this->opaqueIndex = 0;
    this->alphaTestIndex = this->size();
    this->transparentIndex = this->size();
}

void DrawList::DrawRangeHelper(Gfx::CommandBuffer& cmd, int from, int to, std::optional<Gfx::PolygonMode> polygonModeOverride, std::optional<Gfx::PipelineConfig::PipelineConfig_t::Stencil> stencilOverride) const
{
    for (int i = from; i < to; ++i)
    {
        auto& draw = this->at(sorted.at(i));
        auto shaderProgram = draw.material->GetShaderProgram();
        if (shaderProgram)
        {
            if (draw.materialSet != -1 && draw.materialResource)
                cmd.BindResource(draw.materialSet, draw.materialResource);
            if (draw.objectSet && draw.objectResource)
                cmd.BindResource(draw.objectSet, draw.objectResource);

            bool configModified = polygonModeOverride.has_value() || stencilOverride.has_value();
            if (configModified)
            {
                auto modifiedConfig = **draw.shaderConfig;
                if (polygonModeOverride.has_value())
                    modifiedConfig.polygonMode = polygonModeOverride.value();
                if (stencilOverride.has_value())
                    modifiedConfig.stencil = stencilOverride.value();
                cmd.BindShaderProgram(shaderProgram, modifiedConfig);
            }
            else
            {
                cmd.BindShaderProgram(shaderProgram, *draw.shaderConfig);
            }

            auto ps = draw.GetPushConstant();
            cmd.SetPushConstant(shaderProgram, (void*)&ps);
            cmd.Draw(draw.indexCount, 1, 0, 0);
        }
    }
}

void DrawList::SortByDistance(const glm::vec3& cameraPos)
{
    std::sort(
        sorted.begin(),
        sorted.end(),
        [&cameraPos, this](int left, int right)
        {
            return glm::distance2(cameraPos, glm::vec3(this->at(left).model[3])) <
                   glm::distance2(cameraPos, glm::vec3(this->at(right).model[3]));
        }
    );
}
} // namespace Rendering
