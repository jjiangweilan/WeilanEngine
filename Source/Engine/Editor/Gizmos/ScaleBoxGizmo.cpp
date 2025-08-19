#include "ScaleBoxGizmo.hpp"
#include "Core/Component/Camera.hpp"
#include "ThirdParty/imgui/imgui.h"

void ScaleBoxGizmo::ProcessUserInput(const float3& position, const glm::quat& rotation, float3& inoutSize)
{
    this->position = position;
    this->rotation = rotation;

    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    for (int handleIdx = 0; handleIdx < 6; ++handleIdx)
    {
        if (isMouseClicked)
        {
            camera->ScreenUVToWorldSpaceRay();
        }
    }
};

void ScaleBoxGizmo::Draw(Gfx::CommandBuffer& cmd) {};
