#include "./GizmoRenderer.hpp"

void GizmoRenderer::Draw(GizmoContext& context, Gfx::CommandBuffer& cmd)
{
    for (const auto& gizmo : context.GetActiveGizmos())
    {
        gizmo->SetupDraw(perScene);
        gizmo->Draw(cmd);
    }
}
