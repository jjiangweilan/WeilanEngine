#include "GameSceneWindow.hpp"
#include "../imgui/imgui.h"

namespace Engine::Editor
{
    void GameSceneWindow::Tick(RefPtr<Gfx::Image> image)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2, 0.2, 0.2, 1));
        ImGui::Begin("Game Scene", nullptr, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        {
            const auto& sceneDesc = image->GetDescription();
            float w = sceneDesc.width;
            float h = sceneDesc.height;
            float whRatio = sceneDesc.width / (float)sceneDesc.height;
            ImVec2 regMax = ImGui::GetWindowContentRegionMax();
            ImVec2 regMin = ImGui::GetWindowContentRegionMin();
            ImVec2 size = {regMax.x - regMin.x, regMax.y - regMin.y};
 
            // calculate size and pos
            float startingY = ImGui::GetCursorStartPos().y;
            float startingX = ImGui::GetCursorStartPos().x;
            if (w > size.x)
            {
                w = size.x;
                h = size.x / whRatio;
                ImGui::SetCursorPos(ImVec2(regMin.x - 1, startingY + ((regMax.y - startingY)) / 2.0f - h / 2.0f));
            }
            if (h > size.y)
            {
                h = size.y;
                w = size.y * whRatio;
                ImGui::SetCursorPos(ImVec2(startingX + (regMax.x - regMin.x) / 2.0f - w / 2.0f, startingY - 2));
            }
            size = ImVec2(w, h);

            ImGui::Image(image.Get(), size, ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(0.3, 0.3, 0.3, 1));
        }

        ImGui::End();
    }
}
