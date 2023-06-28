#include "AssetDatabase/Asset.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

using namespace Engine;
using namespace Engine::RenderGraph;

namespace Engine::RenderGraph
{
class RenderGraphUnitTest : public Graph
{
public:
    void TestGraph()
    {
        Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, {.windowSize = {1920, 1080}});

        std::unique_ptr<RenderGraphUnitTest> graph = std::make_unique<RenderGraphUnitTest>();

        ShaderCompiler shaderCompiler;
        try
        {
            shaderCompiler.Compile(shader, true);
        }
        catch (ShaderCompiler::CompileError e)
        {
            SPDLOG_ERROR(e.what());
            ASSERT_TRUE(false);
        }
        auto& vertSPV = shaderCompiler.GetVertexSPV();
        auto& fragSPV = shaderCompiler.GetFragSPV();
        shaderProgram =
            GetGfxDriver()->CreateShaderProgram("shaderTest", &shaderCompiler.GetConfig(), vertSPV, fragSPV);

        RenderNode* genUV = graph->AddNode(
            [this](auto& a, auto& b, auto& c) { GenUV(a, b, c); },
            {{.name = "sampleOut",
              .handle = 0,
              .type = ResourceType::Image,
              .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
              .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
              .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
              .imageLayout = Gfx::ImageLayout::Color_Attachment,
              .externalImage = GetGfxDriver()->GetSwapChainImageProxy().Get()}},
            {},
            {},
            {0},
            {{.colors = {{
                  .handle = 0,
                  .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                  .loadOp = Gfx::AttachmentLoadOperation::DontCare,
                  .storeOp = Gfx::AttachmentStoreOperation::Store,
              }}}}
        );

        graph->Process(&genUV->GetOutputPorts()[0]);

        bool shouldBreak = false;
        SDL_Event sdlEvent;
        while (!shouldBreak)
        {
            while (SDL_PollEvent(&sdlEvent))
            {
                switch (sdlEvent.type)
                {
                    case SDL_KEYDOWN:
                        if (sdlEvent.key.keysym.scancode == SDL_SCANCODE_Q)
                        {
                            shouldBreak = true;
                        }
                }
            }

            graph->Execute();
        }

        GetGfxDriver()->WaitForIdle();
        graph = nullptr;
        shaderProgram = nullptr;
        Gfx::GfxDriver::DestroyGfxDriver();
        return;
    }

    void GenUV(CommandBuffer& cmdBuf, Gfx::RenderPass& renderPass, const RenderPass::Buffers& buffers)
    {
        Gfx::ClearValue clears = {.color = {0, 0, 0, 0}};
        cmdBuf.BeginRenderPass(&renderPass, {clears});
        cmdBuf.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmdBuf.Draw(3, 1, 0, 0);
        cmdBuf.EndRenderPass();
    }

    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
    std::string shader{R"(
#version 450

#if CONFIG
name: TestShader
#endif

#if VERT
layout(location = 0) out vec2 o_uv;

vec4 vertices[3] = {
    {-1, -3, 0.5, 1.0},
    {-1, 1, 0.5, 1.0},
    {3, 1, 0.5, 1.0}
};

void main()
{
    vec4 pos = vertices[gl_VertexIndex];
    gl_Position = pos;
    o_uv = pos.xy * 0.5 + 0.5;
}
#endif

#if FRAG
layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 color;
void main()
{
    color = vec4(i_uv, 0, 1);
}
#endif
)"};
};
} // namespace Engine::RenderGraph
TEST(RenderGraph, Process)
{
    RenderGraphUnitTest t;

    try
    {
        t.TestGraph();
    }
    catch (std::logic_error e)
    {
        SPDLOG_ERROR(e.what());
        ASSERT_TRUE(false);
    }
}
