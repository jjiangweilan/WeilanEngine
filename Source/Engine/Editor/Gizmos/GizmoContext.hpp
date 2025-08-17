#pragma once
#include "./GizmoBase.hpp"
#include "./GizmoHandle.hpp"
#include "Libs/Math.hpp"
#include <unordered_map>

struct GizmoState
{
    bool isHot = false;
    std::function<void()> draw;
};

class GizmoContext
{
public:
    using GizmoList = std::vector<GizmoState>;

    void DrawScaleBox(GizmoHandle& handle, const float3& position, float3& inoutSize);

    const GizmoList& GetActiveGizmos() { return allGizmos; }

    void ClearInactiveGizmos();

    GizmoState GetGizmoState(GizmoHandle& handle);
    bool IsHandleCreated(GizmoHandle& handle);

private:
    GizmoList allGizmos;
    GizmoList freeGizmos;

    void GetHandleID(uint32_t& outID, uint32_t& outGeneration);
};
