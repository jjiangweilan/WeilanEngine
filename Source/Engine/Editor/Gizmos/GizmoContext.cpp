#include "GizmoContext.hpp"
#include "./ScaleBoxGizmo.hpp"
#include "ThirdParty/imgui/imgui.h"

GizmoContext::GizmoContext() {}

void GizmoContext::ClearInactiveGizmos() {}

void GizmoContext::GetHandleID(uint32_t& outID, uint32_t& outGeneration) {}

bool GizmoContext::ValidateGizmoHandle(GizmoHandle& handle)
{
    if (!handle.IsValid())
    {
        inactiveGizmos->push_back(GizmoState{});
        handle.selfNode = --inactiveGizmos->end();
    }

    // If this handle is already activated, then we can't draw it again, return false to prevent it
    if (handle.IsActive())
        return false;

    handle.selfNode->active = true;
    activeGizmos->splice(activeGizmos->end(), *inactiveGizmos, handle.selfNode);

    return true;
}
