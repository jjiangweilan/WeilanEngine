#include "Editor/Renderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <SDL2/SDL.h>
#include <gtest/gtest.h>

TEST(GameEditor, Render)
{
    Engine::Extent2D windowSize{960, 540};
    auto gfxDriver = Engine::Gfx::GfxDriver::CreateGfxDriver(Engine::Gfx::Backend::Vulkan, {.windowSize = windowSize});

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(Engine::GetGfxDriver()->GetSDLWindow());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    auto graph = std::make_unique<Engine::RenderGraph::Graph>();
    auto cmd = Engine::GetGfxDriver()->CreateCommandPool({});
    auto renderPipeline = std::make_unique<Engine::RenderPipeline>();
    auto r = std::make_unique<Engine::Editor::Renderer>();
    auto [editorRenderNode, editorRenderOutputHandle] = r->BuildGraph(*graph);
    graph->Process(editorRenderNode, editorRenderOutputHandle);

    bool quit = false;
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();

    Engine::ImmediateGfx::OnetimeSubmit(
        [&graph, windowSize](Engine::Gfx::CommandBuffer& cmd)
        {
            cmd.SetViewport(
                {.x = 0,
                 .y = 0,
                 .width = (float)windowSize.width,
                 .height = (float)windowSize.height,
                 .minDepth = 0,
                 .maxDepth = 1}
            );
            Engine::Rect2D rect = {{0, 0}, {windowSize.width, windowSize.height}};
            cmd.SetScissor(0, 1, &rect);
            graph->Execute(cmd);
        }
    );
}
