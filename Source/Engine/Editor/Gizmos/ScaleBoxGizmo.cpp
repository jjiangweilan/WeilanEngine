#include "ScaleBoxGizmo.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Math/GeometryRendering.hpp"
#include "GamePlay/Input.hpp"
#include "Libs/Math.hpp"
#include "Rendering/Graphics.hpp"
#include "ThirdParty/imgui/imgui.h"

void ScaleBoxGizmo::ProcessUserInput(float3& position, const glm::quat& rotation, float3& inoutSize)
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
                previousMousePosVS = camera->ScreenUVToCameraNearPlaneInViewSpace(uv);
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

        float3 mousePosVS = camera->ScreenUVToCameraNearPlaneInViewSpace(uv);
        float3 moveDelta_v = mousePosVS - previousMousePosVS;
        previousMousePosVS = mousePosVS;

        moveDelta_v.z = 0;
        dir_v.z = 0;

        dir_v = glm::normalize(dir_v);
        float t = glm::dot(dir_v, moveDelta_v);

        // move handlePos
        inoutSize += glm::abs(dirs[activeHandle]) * t * 60.f;
        position += dirs[activeHandle] * t * 30.f;
    }
};

void ScaleBoxGizmo::Draw(Gfx::CommandBuffer& cmd)
{
    Submesh* cube = EngineInternalResources::GetCubeMesh();
    Gfx::ShaderProgram* program = forwardLitShader->GetShaderProgram();

    float3 dirs[6] = {
        float3(-1, 0, 0),
        float3(1, 0, 0),
        float3(0, -1, 0),
        float3(0, 1, 0),
        float3(0, 0, -1),
        float3(0, 0, 1),

    };

    cmd.BindResource(0, perScene);
    for (int i = 0; i < 6; ++i)
    {
        glm::mat4 finalM = GetBoxTransformMatrix(dirs[i]);

        cmd.BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
        cmd.BindVertexBuffer(cube->GetGfxVertexBufferBindings(), 0);
        cmd.SetPushConstant(program, &finalM);
        cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
        cmd.DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
    }

    // TODO(perf)
    Box box(position, extent * 2.0f, rotation);
    GeometryRendering::DrawWireBox(cmd, box);
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
