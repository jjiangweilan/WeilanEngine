#include "EnvironmentBaker.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
EnvironmentBaker::EnvironmentBaker() {}

void EnvironmentBaker::CreateRenderData(uint32_t width, uint32_t height)
{
    GetGfxDriver()->WaitForIdle();
    sceneImage = GetGfxDriver()->CreateImage(
        {
            .width = width,
            .height = height,
            .format = Gfx::ImageFormat::R8G8B8A8_SRGB,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
            .isCubemap = false,
        },
        Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst
    );

    if (renderGraph == nullptr)
    {
        renderGraph = std::make_unique<RenderGraph::Graph>();
        Gfx::ClearValue clear;
        clear.color = {0, 0, 0, 0};
        renderGraph->AddNode(
            [clear](Gfx::CommandBuffer& cmd, auto& pass, auto& res)
            {
                cmd.BeginRenderPass(&pass, {clear});
                cmd.EndRenderPass();
            },
            {
                {
                    .name = "image",
                    .handle = 0,
                    .type = RenderGraph::ResourceType::Image,
                    .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                    .stageFlags = Gfx::PipelineStage::Fragment_Shader,
                    .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                    .imageLayout = Gfx::ImageLayout::Color_Attachment,
                    .externalImage = sceneImage.get(),
                },
            },
            {
                {
                    .colors = {{
                        .handle = 0,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .loadOp = Gfx::AttachmentLoadOperation::Clear,
                        .storeOp = Gfx::AttachmentStoreOperation::Store,
                    }},
                },
            }
        );
    }
}

void EnvironmentBaker::Render(Gfx::CommandBuffer& cmd)
{
    auto cubemap = GetGfxDriver()->CreateImage(
        {
            .width = (uint32_t)size.x,
            .height = (uint32_t)size.y,
            .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
            .mipLevels = 1,
            .isCubemap = true,
        },
        Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::ColorAttachment | Gfx::ImageUsage::Texture
    );
};

void EnvironmentBaker::Bake() {}

bool EnvironmentBaker::Tick()
{
    bool open = true;

    ImGui::Begin("Environment Baker", &open);
    // create scene color if it's null or if the window size is changed
    auto contentMax = ImGui::GetWindowContentRegionMax();
    auto contentMin = ImGui::GetWindowContentRegionMin();
    float width = contentMax.x - contentMin.x;
    float height = contentMax.y - contentMin.y;
    if (sceneImage == nullptr || sceneImage->GetDescription().width != (uint32_t)width ||
        sceneImage->GetDescription().height != (uint32_t)height)
    {
        // CreateRenderData(width, height);
    }

    ImGui::InputInt2("resolution", &size[0]);
    if (ImGui::Button("Bake"))
    {
        Bake();
    }

    ImGui::End();
    return open;
}

void EnvironmentBaker::Open() {}
void EnvironmentBaker::Close()
{
    sceneImage = nullptr;
}

} // namespace Engine::Editor
