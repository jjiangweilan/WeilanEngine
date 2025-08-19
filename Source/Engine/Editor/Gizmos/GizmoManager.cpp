#include "./GizmoManager.hpp"
#include "./MeshGizmo.hpp"
#include "./ScaleBoxGizmo.hpp"
#include "ThirdParty/imgui/imgui.h"

GizmoManager::GizmoManager() {}

void GizmoManager::ClearInactiveGizmos()
{
    for (auto& g : *activeGizmos)
    {
        g.active = false;
    }

    for (auto& g : *inactiveGizmos)
    {
        *g.isHandleValid = false;
    }
    inactiveGizmos->clear();

    std::swap(activeGizmos, inactiveGizmos);
}

void GizmoManager::GetHandleID(uint32_t& outID, uint32_t& outGeneration) {}

bool GizmoManager::ValidateGizmoHandle(GizmoHandle& handle)
{
    // If this handle is already activated, then we can't draw it again, return false to prevent it
    if (handle.IsActive())
        return false;

    if (!handle.IsValid())
    {
        inactiveGizmos->push_back(GizmoState{});
        handle.selfNode = --inactiveGizmos->end();
    }

    handle.selfNode->active = true;
    handle.selfNode->isHandleValid = handle.isNodeValid;
    *handle.isNodeValid = true;

    activeGizmos->splice(activeGizmos->end(), *inactiveGizmos, handle.selfNode);

    return true;
}

void GizmoManager::DrawMesh(
    GizmoHandle& handle, Mesh* mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix
)
{
    Draw<GizmoDrawMesh>(handle, mesh, submeshIndex, shader, modelMatrix);
}

void GizmoManager::DrawMesh(
    GizmoHandle& handle, Mesh* mesh, int submeshIndex, Material* shader, const glm::mat4& modelMatrix
)
{
    Draw<GizmoDrawMesh>(handle, mesh, submeshIndex, shader, modelMatrix);
}
