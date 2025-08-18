#pragma once
#include "GizmoBase.hpp"

class ScaleBoxGizmo : public GizmoBase
{
public:
    ScaleBoxGizmo() {}

    void ProcessUserInput(const float3& position, float3& inoutSize) {};
    virtual void Draw(Gfx::CommandBuffer& cmd) {};

    float3 position;
    float3 size;
};
