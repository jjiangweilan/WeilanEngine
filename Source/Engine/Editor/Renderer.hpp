#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <tuple>

namespace Engine::Editor
{
class Renderer
{
public:
    // customFont: a path to a font file on disk
    Renderer(const char* customFont = nullptr);
    ~Renderer();
    std::tuple<RenderGraph::RenderNode*, RenderGraph::ResourceHandle> BuildGraph(
        RenderGraph::Graph& graph,
        RenderGraph::RenderNode* colorNode = nullptr,
        RenderGraph::ResourceHandle colorHandle = 0,
        RenderGraph::RenderNode* depthNode = nullptr,
        RenderGraph::ResourceHandle depthHandle = 0
    );

private:
    std::unique_ptr<RenderGraph::Graph> editorRenderGraph;
    std::unique_ptr<Gfx::Buffer> indexBuffer;
    std::unique_ptr<Gfx::Buffer> vertexBuffer;
    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
    std::unique_ptr<Gfx::Image> fontImage;

    void RenderEditor(Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res);
};

} // namespace Engine::Editor
