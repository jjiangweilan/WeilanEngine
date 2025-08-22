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
    bool anyHandleActive = false;

    for (int handleIdx = 0; handleIdx < 6; ++handleIdx)
    {
        anyHandleActive = anyHandleActive || handles[handleIdx].isActive;
    }

    bool isMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool IsMouseDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    float3 dir[6] = {
        float3(-1, 0, 0),
        float3(1, 0, 0),
        float3(0, -1, 0),
        float3(0, 1, 0),
        float3(0, 0, -1),
        float3(0, 0, 1),

    };

    if (isMouseClicked)
    {
        for (int handleIdx = 0; handleIdx < 6; ++handleIdx)
        {
            auto uv = editorContext->GetUVInSceneView();
            auto camera = editorContext->GetEditorCamera();
            auto ray = camera->ScreenUVToWorldSpaceRay(uv);

            bool activate = false;
            for (int i = 0; i < 6; ++i)
            {
                float distance = -1;
                activate = RayVsBox(ray, handles[i].box.Transform(GetBoxTransformMatrix(dir[i])), distance);

                if (activate)
                    break;
            }

            if (activate)
            {
                MarkActive();
                spdlog::info("ScaleBoxGizmo activated");
            }
            else
            {
                spdlog::info("ScaleBoxGizmo deactivated");
            }
        }
    }

    if (IsMouseDragging)
    {}
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
        glm::mat4 finalM = GetBoxTransformMatrix(dir[i]);

        cmd.BindResource(0, perScene);
        cmd.BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
        cmd.BindVertexBuffer(cube->GetGfxVertexBufferBindings(), 0);
        cmd.SetPushConstant(program, &finalM);
        cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
        cmd.DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
    }
};

float4x4 ScaleBoxGizmo::GetBoxTransformMatrix(const float3& dir)
{
    float4x4 m = glm::translate(glm::mat4(1), position);

    float4x4 ml = glm::mat4_cast(rotation) * glm::translate(glm::mat4(1), dir * extent) *
                  glm::scale(glm::mat4(1), float3(0.1, 0.1, 0.1));

    return m * ml;
}
