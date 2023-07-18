#pragma once
#include "Core/Component/Light.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/Shaders.hpp"

namespace Engine
{
class Scene;
class Transform;
class DualMoonGraph : public RenderGraph::Graph
{
public:
    DualMoonGraph(Scene& scene, Shader& opaqueShader, Shader& shadowShader);

    void Execute(Gfx::CommandBuffer& cmd) override;
    void Process() override;
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
    struct LightInfo
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
        LightInfo lights[MAX_LIGHT_COUNT];
    } sceneInfo;
    std::unique_ptr<Gfx::ShaderResource> sceneShaderResource;

    using DrawList = std::vector<SceneObjectDrawData>;
    DrawList drawList;
    Scene& scene;
    Shader* opaqueShader;
    Shader* shadowShader;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    RenderGraph::RenderNode* colorOutput;
    RenderGraph::ResourceHandle colorHandle;
    RenderGraph::RenderNode* depthOutput;
    RenderGraph::ResourceHandle depthHandle;

    RenderGraph::RenderNode* shadowPass;

    void AppendDrawData(Transform& transform, std::vector<SceneObjectDrawData>& drawList);

    void ProcessLights(Scene* gameScene);
    void BuildGraph();
};
} // namespace Engine
