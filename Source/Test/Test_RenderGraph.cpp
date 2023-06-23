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

        RenderGraphUnitTest graph;

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

        RenderNode* genUV = graph.AddNode(
            [this](auto& a, auto& b, auto& c) { GenUV(a, b, c); },
            {{
                .name = "uvOut",
                .handle = 0,
                .type = ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .imageCreateInfo =
                    {
                        .width = 256,
                        .height = 256,
                        .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                    },
            }},
            {},
            {},
            {0},
            {.colors = {{
                 .handle = 0,
                 .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                 .loadOp = Gfx::AttachmentLoadOperation::DontCare,
                 .storeOp = Gfx::AttachmentStoreOperation::Store,
             }}}

        );

        RenderNode* sampleUV = graph.AddNode(
            SampleUV,
            {{.name = "uvTex",
              .handle = 0,
              .type = ResourceType::Image,
              .accessFlags = Gfx::AccessMask::Shader_Read,
              .stageFlags = Gfx::PipelineStage::Fragment_Shader,
              .imageUsagesFlags = Gfx::ImageUsage::Texture,
              .imageLayout = Gfx::ImageLayout::Shader_Read_Only},
             {
                 .name = "sampleOut",
                 .handle = 1,
                 .type = ResourceType::Image,
                 .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                 .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                 .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                 .imageLayout = Gfx::ImageLayout::Color_Attachment,
                 .imageCreateInfo =
                     {
                         .width = 256,
                         .height = 256,
                         .format = Gfx::ImageFormat::R8G8B8A8_UNorm,
                         .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                         .mipLevels = 1,
                     },
             }},
            {1},
            {0},
            {1},
            {.colors = {{
                 .handle = 1,
                 .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                 .loadOp = Gfx::AttachmentLoadOperation::DontCare,
                 .storeOp = Gfx::AttachmentStoreOperation::Store,
             }}}
        );

        RenderNode::Connect(genUV->GetOutputPorts()[0], sampleUV->GetInputPorts()[0]);

        graph.Process();
        graph.Execute();

        Gfx::GfxDriver::DestroyGfxDriver();
        return;
    }

    void GenUV(CommandBuffer& cmdBuf, Gfx::RenderPass& renderPass, const RenderPass::Buffers& buffers)
    {
        Gfx::ClearValue clears = {.color = {0, 0, 0, 0}};
        cmdBuf.BeginRenderPass(&renderPass, {clears});
        cmdBuf.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmdBuf.DrawIndexed(3, 1, 0, 0, 0);
        cmdBuf.EndRenderPass();
    }

    static void SampleUV(CommandBuffer&, Gfx::RenderPass& renderPass, const RenderPass::Buffers& buffers) {}
    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
    std::string shader{R"(
#version 450 core

#if CONFIG
name: TestShader
#endif

#if VERT
layout(location = 0) out vec2 o_uv;

vec4 vertices[3] = {
    {-1, -3, 0.5, 1.0},
    {-1, -1, 0.5, 1.0},
    {3, -1, 0.5, 1.0}
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
    t.TestGraph();
}
