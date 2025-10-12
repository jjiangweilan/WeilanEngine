#pragma once
#include "Core/Component/MeshRenderer.hpp"
#include "Rendering/Material.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace Rendering
{
struct SceneObjectDrawData
{
    SceneObjectDrawData() = default;
    SceneObjectDrawData(SceneObjectDrawData&& other) = default;
    SceneObjectDrawData& operator=(SceneObjectDrawData&& other) = default;
    bool skinned = false;
    Shader* shader = nullptr;
    const Gfx::PipelineConfig* shaderConfig = nullptr;
    Material* material = nullptr;
    int materialSet = 0;
    int objectSet = -1;
    Gfx::ShaderResource* materialResource = nullptr;
    Gfx::ShaderResource* objectResource = nullptr;
    Gfx::Buffer* indexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType;
    std::vector<Gfx::VertexBufferBinding> vertexBufferBinding;
    uint32_t indexCount;
    glm::float4x4 model;
    glm::float4x4 invTspModel;
    std::array<std::byte, 128> GetPushConstant() const
    {
        std::array<std::byte, 128> data;
        memcpy(&data[0], &model[0], sizeof(glm::float4x4));
        memcpy(&data[0] + sizeof(glm::float4x4), &invTspModel[0], sizeof(glm::float4x4));
        return data;
    }
};
void swap(SceneObjectDrawData&& a, SceneObjectDrawData&& b);

class DrawList : public std::vector<SceneObjectDrawData>
{
public:
    int opaqueIndex;
    int alphaTestIndex;
    int transparentIndex;
    void Add(std::span<MeshRenderer*> meshRenderers);
    void Add(MeshRenderer& meshRenderer);
    void Sort(const glm::vec3& cameraPos);
    void SortByDistance(const glm::vec3& cameraPos);
    const auto& GetSortedIndices() const { return sorted; }
    void Lock();

    void DrawRangeHelper(Gfx::CommandBuffer& cmd, int from, int to) const;

    std::vector<int> sorted;
};
} // namespace Rendering
