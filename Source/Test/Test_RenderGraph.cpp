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
    struct RGBA32
    {
        unsigned char x, y, z, w;
    };
    void TestGraph()
    {
        auto gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, {.windowSize = {640, 480}});
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
            {
                {
                    .name = "sampleOut",
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
                            .isCubemap = false,
                        },
                },
            },
            {
                {
                    .colors =
                        {
                            {
                                .handle = 0,
                                .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                                .loadOp = Gfx::AttachmentLoadOperation::DontCare,
                                .storeOp = Gfx::AttachmentStoreOperation::Store,
                            },
                        },
                },
            }
        );

        auto readbackBuf = GetGfxDriver()->CreateBuffer(
            {.usages = Gfx::BufferUsage::Transfer_Dst,
             .size = 256 * 256 * sizeof(RGBA32),
             .visibleInCPU = true,
             .debugName = "readbackBuf"}
        );
        RGBA32 readbackBufCPU[256 * 256];

        RenderNode* readSample = graph->AddNode(
            [](Gfx::CommandBuffer& cmd, auto& b, const ResourceRefs& res)
            {
                auto buf = (Gfx::Buffer*)res.at(0)->GetResource();
                auto img = (Gfx::Image*)res.at(1)->GetResource();

                Gfx::BufferImageCopyRegion copyRegion[1];
                copyRegion[0].srcOffset = 0;
                copyRegion[0].offset = {0, 0, 0};
                copyRegion[0].extend = {256, 256, 1};
                copyRegion[0].layers = {
                    .aspectMask = Gfx::ImageAspect::Color,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };

                cmd.CopyImageToBuffer(img, buf, copyRegion);
            },
            {
                {
                    .name = "readback sample",
                    .handle = 0,
                    .type = ResourceType::Buffer,
                    .accessFlags = Gfx::AccessMask::Transfer_Write,
                    .stageFlags = Gfx::PipelineStage::Transfer,
                    .externalBuffer = readbackBuf.Get(),
                },
                {
                    .name = "input image",
                    .handle = 1,
                    .type = ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::Transfer_Read,
                    .stageFlags = Gfx::PipelineStage::Transfer,
                    .imageUsagesFlags = Gfx::ImageUsage::TransferSrc,
                    .imageLayout = Gfx::ImageLayout::Transfer_Src,
                },
            },
            {}
        );

        Graph::Connect(genUV, 0, readSample, 1);

        graph->Process();

        ImmediateGfx::OnetimeSubmit(
            [&graph](Gfx::CommandBuffer& cmd)
            {
                cmd.SetViewport({.x = 0, .y = 0, .width = 256, .height = 256, .minDepth = 0, .maxDepth = 1});
                Rect2D rect = {{0, 0}, {(uint32_t)256, (uint32_t)256}};
                cmd.SetScissor(0, 1, &rect);
                graph->Execute(cmd);
            }
        );

        memcpy((void*)readbackBufCPU, readbackBuf->GetCPUVisibleAddress(), 256 * 256 * sizeof(RGBA32));
        EXPECT_TRUE(readbackBufCPU[128 + 128 * 256].x == 255);
        EXPECT_TRUE(readbackBufCPU[128 + 128 * 256].y == 0);
        EXPECT_TRUE(readbackBufCPU[128 + 128 * 256].z == 255);
        EXPECT_TRUE(readbackBufCPU[128 + 128 * 256].w == 0);

        EXPECT_TRUE(readbackBufCPU[129 + 129 * 256].x == 129);
        EXPECT_TRUE(readbackBufCPU[129 + 129 * 256].y == 129);
        EXPECT_TRUE(readbackBufCPU[129 + 129 * 256].z == 0);
        EXPECT_TRUE(readbackBufCPU[129 + 129 * 256].w == 255);

        readbackBuf = nullptr;
        graph = nullptr;
        shaderProgram = nullptr;
        return;
    }

    void GenUV(Gfx::CommandBuffer& cmdBuf, Gfx::RenderPass& renderPass, const ResourceRefs& res)
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
    if (gl_FragCoord.x == 128.5 && gl_FragCoord.y == 128.5)
    {
        color = vec4(1, 0, 1, 0);
    }
    else 
    {
        color = vec4(i_uv, 0, 1);
    }
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