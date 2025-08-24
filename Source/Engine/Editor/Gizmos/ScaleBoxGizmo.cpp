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

    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool isMouseDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    auto camera = editorContext->GetEditorCamera();
    auto uv = editorContext->GetMouseUVInSceneView();

    float3 dirs[6] = {
        float3(-1, 0, 0),
        float3(1, 0, 0),
        float3(0, -1, 0),
        float3(0, 1, 0),
        float3(0, 0, -1),
        float3(0, 0, 1),

    };

    if (isMouseDown && activeHandle == -1)
    {
        auto ray = camera->ScreenUVToWorldSpaceRay(uv);

        for (int handleIdx = 0; handleIdx < 6; ++handleIdx)
        {
            bool activate = false;
            float distance = -1;
            activate =
                RayVsBox(ray, handles[handleIdx].box.Transform(GetBoxTransformMatrix(dirs[handleIdx])), distance);

            if (activate)
            {
                activeHandle = handleIdx;
                break;
            }
        }
    }

    if (!isMouseDown && !isMouseDragging)
    {
        activeHandle = -1;
    }

    // Update inoutSize
    auto mouseDelta = ImGui::GetMouseDragDelta();
    if (activeHandle != -1 && isMouseDragging)
    {
        // Project hanle
        auto dir = glm::rotate(rotation, dirs[activeHandle]);
        float3 dir_v = camera->GetViewMatrix() * float4(dir, 0.0);
        dir_v = glm::normalize(dir);
        float3 mouseDir_v = glm::normalize(camera->ScreenUVToViewSpace(uv));
        float t = glm::dot(dir_v, mouseDir_v);
        spdlog::info("{}", t);
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

void ScaleBoxGizmo::UpdateHandleState(float scale)
{
    for (int i = 0; i < 6; ++i)
    {}
}
