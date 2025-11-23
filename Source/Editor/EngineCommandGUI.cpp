#include "EngineCommandGUI.hpp"
#include "EditorGUI.hpp"
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>
namespace Editor
{
void EngineCommandGUI::EditorDraw()
{
    // Check for Ctrl+P
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
    {
        showCommandInput = !showCommandInput;
        if (showCommandInput)
        {
            commandListCache = EngineCommand::Singleton().GetCommandKeys();
            inputBuffer[0] = '\0'; // Clear the buffer when opening
        }
    }

    // Draw the command input if active
    if (showCommandInput)
    {
        DrawCommandInput();
    }
}

void EngineCommandGUI::DrawCommandInput()
{
    // Get the viewport size
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewportSize = viewport->Size;

    // Set window size and position (centered)
    float windowWidth = 600.0f;
    float windowHeight = 80.0f;
    ImVec2 windowPos = ImVec2(
        viewport->Pos.x + (viewportSize.x - windowWidth) * 0.5f,
        viewport->Pos.y + (viewportSize.y - windowHeight) * 0.5f
    );

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);

    // Create a centered window
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Command Input", &showCommandInput, windowFlags))
    {
        ImGui::SetNextItemWidth(-1); // Full width

        // Auto-focus the input field when window opens
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        int selected = -1;
        int firstItem = -1;
        if (EditorGUI::SearchableMenuItems(commandListCache, inputBuffer, selected, firstItem))
        {
            auto& componentName = commandListCache[selected];

            showCommandInput = false;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && firstItem != -1)
        {
            auto& componentName = commandListCache[firstItem];

            showCommandInput = false;
        }

        // Close on Escape
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            showCommandInput = false;
        }
    }
    ImGui::End();
}

void EngineCommandGUI::RemoveBlanks(EngineCommand::CommandList& list)
{
    for (auto& str : list)
    {
        str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
    }
}
} // namespace Editor
