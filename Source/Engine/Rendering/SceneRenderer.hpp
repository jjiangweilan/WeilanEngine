#pragma once
#include "Core/Component/Light.hpp"
#include "Core/Model.hpp"
#include "Rendering/CmdSubmitGroup.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/Shaders.hpp"

namespace Engine
{
class Scene;
class Transform;
class SceneRenderer : protected RenderGraph::Graph
{
public:
    struct BuildGraphConfig
    {
        Gfx::Image* finalImage; // the final image the scene will render to

        // build graph will transform finalImage to a desired layout
        Gfx::ImageLayout layout;
        Gfx::AccessMaskFlags accessFlags;
        Gfx::PipelineStageFlags stageFlags;
    };

public:
    SceneRenderer(AssetDatabase& db);

public:
    void Render(Gfx::CommandBuffer& cmd, Scene& scene);
    void Process() override;
    auto GetFinalSwapchainOutputs()
    {
        return std::tuple{colorOutput, colorHandle, depthOutput, depthHandle};
    }

    void BuildGraph(const BuildGraphConfig& config);

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
        glm::vec4 shadowMapSize;
        LightInfo lights[MAX_LIGHT_COUNT];
    } sceneInfo;
    std::unique_ptr<Gfx::ShaderResource> sceneShaderResource{};
    std::vector<std::unique_ptr<Gfx::ShaderResource>> vsmMipShaderResources{};
    std::vector<std::unique_ptr<Gfx::ImageView>> vsmMipImageViews{};
    uint32_t globalSceneShaderContentHash;

    using DrawList = std::vector<SceneObjectDrawData>;
    DrawList drawList;
    Scene* scene;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;

    Shader* opaqueShader;
    Shader* shadowShader;
    Shader* copyOnlyShader;

    RenderGraph::RenderNode* colorOutput;
    RenderGraph::ResourceHandle colorHandle;
    RenderGraph::RenderNode* depthOutput;
    RenderGraph::ResourceHandle depthHandle;

    RenderGraph::RenderNode* vsmPass;
    std::vector<RenderGraph::RenderNode*> vsmMipmapPasses;

    Model* cube;
    std::unique_ptr<Shader> skyboxShader;
    std::unique_ptr<Gfx::ShaderResource> skyboxPassResource;
    std::unique_ptr<Texture> envMap;
    BuildGraphConfig config;

    void AppendDrawData(Transform& transform, std::vector<SceneObjectDrawData>& drawList);

    void ProcessLights(Scene& gameScene);
};
} // namespace Engine
