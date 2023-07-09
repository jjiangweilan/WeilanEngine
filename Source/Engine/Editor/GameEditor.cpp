#include "GameEditor.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"

namespace Engine::Editor
{
void GameEditor::Tick()
{
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::EndFrame();
}
} // namespace Engine::Editor
