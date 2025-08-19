#include "./GizmoRenderer.hpp"
#include "./GizmoContext.hpp"

void GizmoRenderer::Draw(GizmoContext& context, Gfx::CommandBuffer& cmd)
{
    for (const auto& gizmo : context.GetActiveGizmos())
    {
        gizmo.ptr->SetupDraw(camera, perScene);
        gizmo.ptr->Draw(cmd);
    }
}
