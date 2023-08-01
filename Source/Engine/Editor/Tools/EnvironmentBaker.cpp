#include "EnvironmentBaker.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
EnvironmentBaker::EnvironmentBaker() {}

void EnvironmentBaker::CreateRenderData(uint32_t width, uint32_t height)
{
    GetGfxDriver()->WaitForIdle();
    renderer = std::make_unique<SceneRenderer>();
    scene =
        std::unique_ptr<Scene>(new Scene({.render = [this](Gfx::CommandBuffer& cmd) { this->renderer->Execute(cmd); }})
        );
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

    renderer->BuildGraph(
        *scene,
        {
            .finalImage = *sceneImage,
            .layout = Gfx::ImageLayout::Shader_Read_Only,
            .accessFlags = Gfx::AccessMask::Shader_Read,
            .stageFlags = Gfx::PipelineStage::Fragment_Shader,
        }
    );
    renderer->Process();
}

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
        CreateRenderData(width, height);
    }

    // render
    ImmediateGfx::OnetimeSubmit([this](Gfx::CommandBuffer& cmd) { this->scene->Render(cmd); });

    ImGui::Image(sceneImage.get(), {width, height});

    ImGui::End();
    return open;
}

} // namespace Engine::Editor
