#include "AssetDatabase/Asset.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/ImmediateGfx.hpp"
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
        Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, {.windowSize = {640, 480}});
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
            [this](auto& a, auto& b, const auto& c) { GenUV(a, b, c); },
            {{.name = "sampleOut",
              .handle = 0,
              .type = ResourceType::Image,
              .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
              .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
              .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
              .imageLayout = Gfx::ImageLayout::Color_Attachment,
              .imageCreateInfo =
                  {.width = 256,
                   .height = 256,
                   .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
                   .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                   .mipLevels = 1,
                   .isCubemap = false}}},
            {0},
            {},
            {0},
            {{.colors = {{
                  .handle = 0,
                  .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                  .loadOp = Gfx::AttachmentLoadOperation::DontCare,
                  .storeOp = Gfx::AttachmentStoreOperation::Store,
              }}}}
        );

        RenderNode* readSample = graph->AddNode(
            [](CommandBuffer& cmd, auto& b, const RenderPass::ResourceRefs& res) {},
            {{.name = "sampleOut",
              .handle = 0,
              .type = ResourceType::Buffer,
              .accessFlags = Gfx::AccessMask::Transfer_Write,
              .stageFlags = Gfx::PipelineStage::Transfer,
              .bufferCreateInfo =
                  {
                      .usages = Gfx::BufferUsage::Transfer_Dst,
                      .size = 256 * 256 * 4,
                      .visibleInCPU = true,
                  }}},
            {},
            {},
            {},
            {}
        );

        graph->Process();

        ImmediateGfx::OnetimeSubmit(
            [&graph](CommandBuffer& cmd)
            {
                cmd.SetViewport({.x = 0, .y = 0, .width = 256, .height = 256, .minDepth = 0, .maxDepth = 1});
                Rect2D rect = {{0, 0}, {(uint32_t)256, (uint32_t)256}};
                cmd.SetScissor(0, 1, &rect);
                graph->Execute(cmd);
            }
        );

        graph = nullptr;
        shaderProgram = nullptr;
        Gfx::GfxDriver::DestroyGfxDriver();
        return;
    }

    void GenUV(CommandBuffer& cmdBuf, Gfx::RenderPass& renderPass, const RenderPass::ResourceRefs& res)
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
