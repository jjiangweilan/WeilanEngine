#pragma once
#include "Core/Component/MeshRenderer.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace Rendering
{
struct SceneObjectDrawData
{
    SceneObjectDrawData() = default;
    SceneObjectDrawData(SceneObjectDrawData&& other) = default;
    SceneObjectDrawData& operator=(SceneObjectDrawData&& other) = default;
    Shader* shader = nullptr;
    const Gfx::ShaderConfig* shaderConfig = nullptr;
    Material* material = nullptr;
    Gfx::ShaderResource* shaderResource = nullptr;
    Gfx::Buffer* indexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType;
    std::vector<Gfx::VertexBufferBinding> vertexBufferBinding;
    glm::mat4 pushConstant;
    uint32_t indexCount;
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
};
} // namespace Rendering
