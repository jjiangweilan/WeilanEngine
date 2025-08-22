#pragma once
#include "Core/Math/Geometry.hpp"
#include "GizmoBase.hpp"

class ScaleBoxGizmo : public GizmoBase
{
public:
    ScaleBoxGizmo() { forwardLitShader = ShaderLibrary::GetShader(Shaders::SimpleForwardLit); }

    void ProcessUserInput(const float3& position, const glm::quat& rotation, float3& inoutSize);
    virtual void Draw(Gfx::CommandBuffer& cmd);

private:
    struct DragHandle
    {
        bool isActive = false;
        Box box;
    };

    DragHandle handles[6];
    float3 position;
    glm::quat rotation;
    float3 extent;
    ObjPtr<Shader2> forwardLitShader = nullptr;

    float4x4 GetBoxTransformMatrix(const float3& dir);
    
};
