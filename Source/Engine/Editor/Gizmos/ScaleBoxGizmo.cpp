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
    bool isMouseDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1);
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

    auto ray = camera->ScreenUVToWorldSpaceRay(uv);
    if (isMouseDown && activeHandle == -1)
    {
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
    if (activeHandle != -1 && isMouseDragging)
    {
        float4x4 viewMatrix = camera->GetViewMatrix();
        float3 cameraAxis[] = {camera->GetRight(), camera->GetUp()};
        float3 cameraAxis_v[] = {float3(1, 0, 0), float3(0, 1, 0)};

        // Project handle to world and view space
        auto vec = glm::rotate(rotation, dirs[activeHandle] * extent);
        auto dir = glm::rotate(rotation, dirs[activeHandle]);
        float3 dir_v = viewMatrix * float4(dir, 0.0);

        // Found major axis
        int majorAxis = 0;
        {
            float d0 = glm::abs(glm::dot(float2(dir_v), float2(cameraAxis_v[0])));
            float d1 = glm::abs(glm::dot(float2(dir_v), float2(cameraAxis_v[1])));
            if (d0 > d1)
                majorAxis = 0;
            else
                majorAxis = 1;
        }

        // Define main plane
        float3 planeN = camera->GetForward();
        float3 handlePos = position + vec;
        float planeW = glm::dot(handlePos, planeN);
        Plane plane = { planeN, planeW };

        // Find intersection point of main plane
        float distance = -1;
        if (!RayVsPlane(ray, plane, distance))
        {
            return;
        }
        float3 intersectionPoint = ray.origin + ray.direction * distance;

        // Project intersectionPoint to axis and calculate the diff
        float projectedLength = glm::dot(intersectionPoint - position, dir);
        float diff = projectedLength - glm::length(vec);

        // Move handlePos
        inoutSize += glm::abs(dirs[activeHandle]) * diff;
        position += dirs[activeHandle] * diff * 0.5f;
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
