#include "GameView.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
GameView::GameView(SceneManager& sceneManager) : sceneManager(sceneManager) {}

void GameView::CreateRenderData(uint32_t width, uint32_t height)
{
    uint32_t mainQueueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();

    scene = sceneManager.GetActiveScene();

    if (scene)
    {
        GetGfxDriver()->WaitForIdle();
        renderer = std::make_unique<SceneRenderer>();
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
        renderer->BuildGraph({
            .finalImage = *sceneImage,
            .layout = Gfx::ImageLayout::Shader_Read_Only,
            .accessFlags = Gfx::AccessMask::Shader_Read,
            .stageFlags = Gfx::PipelineStage::Fragment_Shader,
        });
        renderer->Process();
    }
}

void GameView::Render(Gfx::CommandBuffer& cmd)
{
    float width = sceneImage->GetDescription().width;
    float height = sceneImage->GetDescription().height;
    cmd.SetViewport({.x = 0, .y = 0, .width = width, .height = height, .minDepth = 0, .maxDepth = 1});
    Rect2D rect = {{0, 0}, {(uint32_t)width, (uint32_t)height}};
    cmd.SetScissor(0, 1, &rect);
    renderer->Render(cmd, *scene);
}

bool GameView::Tick()
{
    bool open = true;

    ImGui::Begin("Game View", &open);
    // create scene color if it's null or if the window size is changed
    auto contentMax = ImGui::GetWindowContentRegionMax();
    auto contentMin = ImGui::GetWindowContentRegionMin();
    float width = contentMax.x - contentMin.x;
    float height = contentMax.y - contentMin.y;
    bool creationNeeded =
        (scene == nullptr || scene != sceneManager.GetActiveScene() || sceneImage == nullptr ||
         sceneImage->GetDescription().width != (uint32_t)width ||
         sceneImage->GetDescription().height != (uint32_t)height);
    if (creationNeeded && width != 0 && height != 0)
    {
        CreateRenderData(width, height);
    }

    // imgui image
    ImGui::Image(&sceneImage->GetDefaultImageView(), {width, height});

    ImGui::End();
    return open;
}

void GameView::Render() {}

} // namespace Engine::Editor
