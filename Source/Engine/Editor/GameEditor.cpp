#include "GameEditor.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"

namespace Engine::Editor
{

GameEditor::GameEditor()
{
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(GetGfxDriver()->GetSDLWindow());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
};
void GameEditor::Tick()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();
}
} // namespace Engine::Editor
