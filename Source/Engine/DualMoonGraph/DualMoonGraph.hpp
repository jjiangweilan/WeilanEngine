#pragma once
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/Shaders.hpp"

namespace Engine
{
class Scene;
class Transform;
class DualMoonGraph : public RenderGraph::Graph
{
public:
    DualMoonGraph(Scene& scene, Shader& opaqueShader);

    void Execute(Gfx::CommandBuffer& cmd) override;

    auto GetFinalSwapchainOutputs()
    {
        return std::tuple{colorOutput, colorHandle, depthOutput, depthHandle};
    }

private:
    struct SceneObjectDrawData
    {
        Gfx::ShaderProgram* shader = nullptr;
        const Gfx::ShaderConfig* shaderConfig = nullptr;
        Gfx::ShaderResource* shaderResource = nullptr;
        Gfx::Buffer* indexBuffer = nullptr;
        Gfx::IndexBufferType indexBufferType;
        std::vector<Gfx::VertexBufferBinding> vertexBufferBinding;
        glm::mat4 pushConstant;
        uint32_t indexCount;
    };

    static const int MAX_LIGHT_COUNT = 32; // defined in Commom.glsl
    struct Light
    {
        glm::vec4 position;
        float range;
        float intensity;
    };
    struct SceneInfo
    {
        glm::vec4 viewPos;
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewProjection;
        glm::mat4 worldToShadow;
        glm::vec4 lightCount;
        Light lights[MAX_LIGHT_COUNT];
    } sceneInfo;
    std::unique_ptr<Gfx::ShaderResource> sceneShaderResource;

    using DrawList = std::vector<SceneObjectDrawData>;
    DrawList drawList;
    Scene& scene;
    Shader& opaqueShader;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    RenderGraph::RenderNode* colorOutput;
    RenderGraph::ResourceHandle colorHandle;
    RenderGraph::RenderNode* depthOutput;
    RenderGraph::ResourceHandle depthHandle;

    void BuildGraph();
    void AppendDrawData(Transform& transform, std::vector<SceneObjectDrawData>& drawList);
};
} // namespace Engine
