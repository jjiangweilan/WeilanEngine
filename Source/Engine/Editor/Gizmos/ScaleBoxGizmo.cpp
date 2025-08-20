#include "ScaleBoxGizmo.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/EngineInternalResources.hpp"
#include "GamePlay/Input.hpp"
#include "Libs/Math.hpp"
#include "Rendering/Graphics.hpp"
#include "ThirdParty/imgui/imgui.h"

void ScaleBoxGizmo::ProcessUserInput(const float3& position, const glm::quat& rotation, float3& inoutSize)
{
    this->position = position;
    this->rotation = rotation;
    this->extent = inoutSize / 2.0f;

    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    for (int handleIdx = 0; handleIdx < 6; ++handleIdx)
    {
        if (isMouseClicked)
        {
            auto sceneViewPos = editorContext->GetSceneViewPosition();
            auto mousePos = ImGui::GetMousePos();
        }
    }
};

void ScaleBoxGizmo::Draw(Gfx::CommandBuffer& cmd)
{
    Submesh* cube = EngineInternalResources::GetCubeMesh();
    Gfx::ShaderProgram* program = forwardLitShader->GetShaderProgram();

    float3 dir[6] = {
        float3(-1, 0, 0),
        float3(1, 0, 0),
        float3(0, -1, 0),
        float3(0, 1, 0),
        float3(0, 0, -1),
        float3(0, 0, 1),

    };

    for (int i = 0; i < 6; ++i)
    {
        glm::mat4 m = glm::translate(glm::mat4(1), position);

        glm::mat4 ml = glm::mat4_cast(rotation) * glm::translate(glm::mat4(1), dir[i] * extent) *
                       glm::scale(glm::mat4(1), float3(0.1, 0.1, 0.1));

        glm::mat4 finalM = m * ml;

        cmd.BindResource(0, perScene);
        cmd.BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
        cmd.BindVertexBuffer(cube->GetGfxVertexBufferBindings(), 0);
        cmd.SetPushConstant(program, &finalM);
        cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
        cmd.DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
    }
};
