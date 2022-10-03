#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "GfxDriver/GfxBuffer.hpp"
#include "GfxDriver/Image.hpp"
#include "Core/Graphics//Material.hpp"
#include "Core/Graphics//Mesh.hpp"
#include "Core/AssetDatabase/Importers/ShaderImporter.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp" // shaders

using namespace Engine;
using namespace Engine::Gfx;
namespace
{
class Test_GfxDriver : public ::testing::Test
{
protected:

    virtual void SetUp() override
    {
        spdlog::set_level(spdlog::level::info);
        Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan);
        gfxDriver = Gfx::GfxDriver::Instance();
        AssetDatabase::InitSingleton();
        AssetImporter::RegisterImporter("shad", []() { return MakeUnique<Internal::ShaderImporter>(); });
        AssetDatabase::Instance()->LoadInternalAssets();
    }

    ~Test_GfxDriver() override {}

    RefPtr<Engine::Gfx::GfxDriver> gfxDriver;
};

// Dispatch multiple drawcalls and present them shouldn't crash
TEST_F(Test_GfxDriver, MultipleDraw){
    Material material("simple");

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

    // create color and depth
    ImageDescription imageDescription{};
    imageDescription.format = ImageFormat::R16G16B16A16_SFloat;
    imageDescription.width = gfxDriver->GetWindowSize().width;
    imageDescription.height = gfxDriver->GetWindowSize().height;
    imageDescription.multiSampling = MultiSampling::Sample_Count_1;
    imageDescription.mipLevels = 1;
    auto colorImage = gfxDriver->CreateImage(imageDescription, ImageUsage::ColorAttachment | ImageUsage::TransferSrc);

    imageDescription.format = ImageFormat::D16_UNorm;
    auto depthImage = gfxDriver->CreateImage(imageDescription, ImageUsage::DepthStencilAttachment);

    // create frameBuffer and renderPass
    auto renderPass = gfxDriver->CreateRenderPass();
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
    auto frameBuffer = gfxDriver->CreateFrameBuffer(renderPass);
    frameBuffer->SetAttachments({colorImage, depthImage});

    auto swapChainImage = gfxDriver->GetSwapChainImageProxy();
    auto globalShaderResource = gfxDriver->CreateShaderResource(AssetDatabase::Instance()->GetShader("simple")->GetShaderProgram(), Gfx::ShaderResourceFrequency::Global);
    glm::mat4 identity(1);
    globalShaderResource->SetUniform("SceneInfo.viewProjection", &identity);
    globalShaderResource->SetUniform("Transform.model", &identity);

    while(true)
    {
        UniPtr<CommandBuffer> cmd = gfxDriver->CreateCommandBuffer();

        std::vector<Gfx::ClearValue> clears(2);
        clears[0].color = {{0,0,0,0}};
        clears[1].depthStencil.depth = 1;

        Rect2D scissor{{0,0}, gfxDriver->GetWindowSize()};
        cmd->SetScissor(0, 1, &scissor);
        cmd->BindResource(globalShaderResource);
        cmd->BeginRenderPass(renderPass, frameBuffer, clears);
        cmd->BindShaderProgram(material.GetShader()->GetShaderProgram(), material.GetShader()->GetShaderProgram()->GetDefaultShaderConfig());
        cmd->BindResource(material.GetShaderResource());
        cmd->BindIndexBuffer(mesh.GetMeshBindingInfo().indexBuffer, mesh.GetMeshBindingInfo().indexBufferOffset);
        cmd->BindVertexBuffer(mesh.GetMeshBindingInfo().bindingBuffers, mesh.GetMeshBindingInfo().bindingOffsets, mesh.GetMeshBindingInfo().firstBinding);
        cmd->DrawIndexed(mesh.GetVertexDescription().index.count, 1, 0, 0, 0);
        cmd->EndRenderPass();
        cmd->Blit(colorImage, swapChainImage);

        gfxDriver->ExecuteCommandBuffer(std::move(cmd));

        gfxDriver->DispatchGPUWork();
    }
}
}
