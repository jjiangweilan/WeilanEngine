#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GizmoContext.hpp"

class GizmoRenderer
{
public:
    void SetupDraw(Gfx::ShaderResource* perScene) { this->perScene = perScene; }
    void Draw(GizmoContext& context, Gfx::CommandBuffer& cmd);

private:
    Gfx::ShaderResource* perScene;
};
