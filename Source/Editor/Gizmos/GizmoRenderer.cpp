#include "./GizmoRenderer.hpp"
#include "./GizmoManager.hpp"

void GizmoRenderer::Draw(GizmoManager& gizmoManager, Gfx::CommandBuffer& cmd)
{
    for (const auto& gizmo : gizmoManager.GetActiveGizmos())
    {
        gizmo.ptr->SetupDraw(perScene);
        gizmo.ptr->Draw(cmd);
    }
}
