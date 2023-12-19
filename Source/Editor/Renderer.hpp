#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <tuple>

namespace Editor
{
class Renderer
{
public:
    // customFont: a path to a font file on disk
    Renderer(const char* customFont = nullptr);
    ~Renderer();
    void BuildGraph();
    void Execute(Gfx::CommandBuffer& cmd);

private:
    std::unique_ptr<RenderGraph::Graph> editorRenderGraph;
    std::unique_ptr<Gfx::Buffer> indexBuffer;
    std::unique_ptr<Gfx::Buffer> vertexBuffer;
    std::unique_ptr<Gfx::Buffer> stagingBuffer;
    std::unique_ptr<Gfx::Buffer> stagingBuffer2;
    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
    std::unique_ptr<Gfx::Image> fontImage;
    std::unique_ptr<RenderGraph::Graph> graph;

    void RenderEditor(Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res);
    void Process(RenderGraph::RenderNode* presentNode, RenderGraph::ResourceHandle resourceHandle);
};

} // namespace Editor
