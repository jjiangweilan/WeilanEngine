#pragma once
#include "Rendering/RenderGraph/Graph.hpp"

namespace Engine
{
class Scene;
class Transform;
class DualMoonGraph : public RenderGraph::Graph
{
public:
    DualMoonGraph(Scene& scene);

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
    using DrawList = std::vector<SceneObjectDrawData>;
    DrawList drawList;
    Scene& scene;
    RenderGraph::RenderNode* colorOutput;
    RenderGraph::ResourceHandle colorHandle;
    RenderGraph::RenderNode* depthOutput;
    RenderGraph::ResourceHandle depthHandle;

    void BuildGraph();
    void AppendDrawData(Transform& transform, std::vector<SceneObjectDrawData>& drawList);
};
} // namespace Engine
