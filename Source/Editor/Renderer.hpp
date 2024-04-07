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
    Renderer(Gfx::Image* finalImage, Gfx::Image* fontImage);
    ~Renderer();
    void BuildGraph();
    void Execute(Gfx::CommandBuffer& cmd);

private:
    std::unique_ptr<Gfx::Buffer> indexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> vertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> stagingBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> stagingBuffer2 = nullptr;
    std::unique_ptr<Gfx::ShaderProgram> shaderProgram = nullptr;
    Gfx::Image* fontImage = nullptr;
    Gfx::Image* finalImage = nullptr;
    std::unique_ptr<RenderGraph::Graph> graph = nullptr;

    void RenderEditor(Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const RenderGraph::ResourceRefs& res);
    void Process(RenderGraph::RenderNode* presentNode, RenderGraph::ResourceHandle resourceHandle);
};

} // namespace Editor
