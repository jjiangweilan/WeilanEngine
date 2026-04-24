#pragma once
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "GizmoBase.hpp"

class ScaleBoxGizmo : public GizmoBase
{
public:
    ScaleBoxGizmo()
    {
        forwardLitShader = ShaderLibrary::GetShader(Shaders::SimpleForwardLit);
        for (int i = 0; i < 6; ++i)
        {
            handles[i].box.SetSize(1.0f);
        }
    }

    void ProcessUserInput(float3& position, const glm::quat& rotation, float3& inoutSize);
    void Draw(Gfx::CommandBuffer& cmd) override;

    bool IsActive() override { return activeHandle != -1; }

private:
    struct DragHandle
    {
        bool isActive = false;
        Box box{}; // TODO: no need for one box one handle
    };

    int activeHandle = -1;
    DragHandle handles[6];
    float3 position;
    glm::quat rotation;
    float3 extent;
    ObjPtr<Shader> forwardLitShader = nullptr;
    float2 previousMouseDelta = float2(0, 0);

    float4x4 GetBoxTransformMatrix(const float3& dir);
    void UpdateHandleState(float scale);
};
