#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "GfxDriver/Vulkan/VKDriver.hpp"
#include "GfxDriver/GfxFactory.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/Image.hpp"

using namespace Engine;
using namespace Engine::Gfx;
namespace
{
class Test_GfxDriver : public ::testing::Test
{
protected:

    Test_GfxDriver()
    {
        spdlog::set_level(spdlog::level::off);
        gfxDriver = MakeUnique<Engine::Gfx::VKDriver>();
        gfxDriver->Init();
        factory = Gfx::GfxFactory::Instance();
    }

    ~Test_GfxDriver() override {}

    UniPtr<Engine::Gfx::GfxDriver> gfxDriver;
    RefPtr<Engine::Gfx::GfxFactory> factory;
    RefPtr<Engine::Gfx::ShaderLoader> shaderLoader;
};

// Dispatch multiple drawcalls and present them shouldn't crash
TEST_F(Test_GfxDriver, MultipleDraw){
    RenderContext* renderContext;
    std::shared_ptr<RenderTarget> renderTarget;

    Material material("simple");

    renderContext = gfxDriver->GetRenderContext();

    VertexDescription vertDesc;
    float data[] = {
        -0.5, 0, 0.5, 1,
        0.5, 1, 0.5, 1,
        0, -0.5, 0.5, 1,
    };

    float vertColor[] = {
         0.1, 0.5, 0.2, 1,
        0.3, 0.3, 0.6, 1,
        0.1, 1.5, 0.5, 1,
    };

    vertDesc.position.componentCount = 4;
    vertDesc.position.count = 3;
    vertDesc.position.data = data;

    vertDesc.colors.resize(1);
    vertDesc.colors[0].componentCount = 4;
    vertDesc.colors[0].count = 3;
    vertDesc.colors[0].data = (void*)vertColor;
    vertDesc.colors[0].dataByteSize = sizeof(float);

    vertDesc.index.componentCount = 1;
    vertDesc.index.count = 3;
    uint16_t vertIndex[] = {
        0,2,1
    };
    vertDesc.index.data = vertIndex;

    Mesh mesh(vertDesc);

    // render target description
    Engine::RenderTargetDescription rtDescription;
    Extent2D windowSize = gfxDriver->GetWindowSize();
    rtDescription.width = windowSize.width;
    rtDescription.height = windowSize.height;
    Engine::RenderTargetAttachmentDescription rtaColor;
    rtaColor.format = Engine::Gfx::ImageFormat::R16G16B16A16_SFloat;
    rtaColor.multiSampling = Engine::Gfx::MultiSampling::Sample_Count_1;
    rtaColor.mipLevels = 1;
    rtDescription.colorsDescriptions.push_back(rtaColor);
    Engine::RenderTargetAttachmentDescription rtaDepth;
    rtaDepth.format = Engine::Gfx::ImageFormat::D16_UNorm;
    rtaDepth.multiSampling = Engine::Gfx::MultiSampling::Sample_Count_1;
    rtaDepth.mipLevels = 1;
    rtaDepth.clearValue.depthStencil = {1 ,0};
    rtDescription.depthStencilDescription = rtaDepth;

    // create global resource
    auto globalResource = Gfx::GfxFactory::Instance()->CreateShaderResource(ShaderLoader::Instance()->GetShader("Internal/SceneLayout"), Gfx::ShaderResourceFrequency::View);

    // create color and depth
    ImageDescription imageDescription{};
    imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
    imageDescription.width = gfxDriver->GetWindowSize().width;
    imageDescription.height = gfxDriver->GetWindowSize().height;
    imageDescription.multiSampling = MultiSampling::Sample_Count_1;
    imageDescription.mipLevels = 1;
    auto colorImage = Gfx::GfxFactory::Instance()->CreateImage(imageDescription, ImageUsage::ColorAttachment | ImageUsage::TransferSrc);

    imageDescription.format = ImageFormat::D16_UNorm;
    auto depthImage = Gfx::GfxFactory::Instance()->CreateImage(imageDescription, ImageUsage::DepthStencilAttachment);

    // create frameBuffer and renderPass
    auto renderPass = Gfx::GfxFactory::Instance()->CreateRenderPass();
    RenderPass::Attachment colorAttachment;
    colorAttachment.format = ImageFormat::R16G16B16A16_SFloat;
    colorAttachment.multiSampling = MultiSampling::Sample_Count_1;
    colorAttachment.loadOp = AttachmentLoadOperation::Clear;
    colorAttachment.storeOp = AttachmentStoreOperation::Store;

    RenderPass::Attachment depthAttachment;
    depthAttachment.format = ImageFormat::D16_UNorm;
    depthAttachment.multiSampling = MultiSampling::Sample_Count_1;
    depthAttachment.loadOp = AttachmentLoadOperation::Clear;
    depthAttachment.storeOp = AttachmentStoreOperation::DontCare;
    renderPass->SetAttachments({colorAttachment}, depthAttachment);
    RenderPass::Subpass subpass;
    subpass.colors.push_back(0);
    subpass.depthAttachment = 1;
    renderPass->SetSubpass({subpass});
    auto frameBuffer = Gfx::GfxFactory::Instance()->CreateFrameBuffer(renderPass);
    frameBuffer->SetAttachments({colorImage, depthImage});

    renderTarget = gfxDriver->CreateRenderTarget(rtDescription);

    gfxDriver->SetPresentImage(colorImage);

    for(int i = 0; i < 5; ++i)
    {
        std::unique_ptr<CommandBuffer> cmd = renderContext->CreateCommandBuffer();

        RenderPassConfig renderPassConfig(1);
        renderPassConfig.colors[0].loadOp = Engine::Gfx::AttachmentLoadOperation::Clear;
        renderPassConfig.colors[0].storeOp = Engine::Gfx::AttachmentStoreOperation::Store;
        renderPassConfig.depthStencil.loadOp = Engine::Gfx::AttachmentLoadOperation::Clear;
        renderPassConfig.depthStencil.storeOp = Engine::Gfx::AttachmentStoreOperation::DontCare;

        renderContext->BeginFrame(globalResource);

        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;
        
        cmd->BeginRenderPass(renderPass, frameBuffer, clears);
        cmd->Render(mesh, material);
        cmd->EndRenderPass();

        renderContext->EndFrame();

        renderContext->ExecuteCommandBuffer(std::move(cmd));

        gfxDriver->DispatchGPUWork();
    }
}

TEST_F(Test_GfxDriver, Buffer_GPUOnly) {
    unsigned char bytes[] = { 0x3, 0x5, 0x10, 0xff };
    auto buf1 = factory->CreateBuffer(32, Gfx::BufferUsage::Uniform, false);
    buf1->Write(bytes, sizeof(bytes), 0);
    gfxDriver->ForceSyncResources(); // upload data to gpu
    unsigned char* readback = (unsigned char*)buf1->GetCPUVisibleAddress();
    EXPECT_EQ(readback, nullptr);
}

TEST_F(Test_GfxDriver, Buffer_CPUVisible)
{
    // can readback
    auto buf0 = factory->CreateBuffer(32, Gfx::BufferUsage::Uniform, true);
    unsigned char bytes[] = { 0x3, 0x5, 0x10, 0xff };
    buf0->Write(bytes, sizeof(bytes), 0);
    gfxDriver->ForceSyncResources(); // upload data to gpu
    unsigned char* readback = (unsigned char*)buf0->GetCPUVisibleAddress();

    ASSERT_NE(readback, nullptr);
    EXPECT_EQ(readback[0], 0x3);
    EXPECT_EQ(readback[1], 0x5);
    EXPECT_EQ(readback[2], 0x10);
    EXPECT_EQ(readback[3], 0xff);
}

TEST_F(Test_GfxDriver, Buffer_Resize)
{
    auto buf = factory->CreateBuffer(256, Gfx::BufferUsage::Vertex, true);
    buf->Resize(512);
    unsigned char bytes[512];
    memset(bytes, 0, 512);
    bytes[400] = 3;
    bytes[200] = 4;
    buf->Write(bytes, sizeof(bytes), 0);
    auto readBack = (unsigned char*)buf->GetCPUVisibleAddress();
    readBack[400] = 3;
    readBack[200] = 4;
}

TEST_F(Test_GfxDriver, ShaderResource)
{
    auto shaderProgram = gfxDriver->GetShaderLoader()->GetShader("simple");
    auto shaderResource = factory->CreateShaderResource(shaderProgram, Gfx::ShaderResourceFrequency::Material);
    auto v = glm::vec4(0.2, 0.3, 0.4, 0.5);
    shaderResource->SetUniform("Colors", "color0", &v);
}
}
