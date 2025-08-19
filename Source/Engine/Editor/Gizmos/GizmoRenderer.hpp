#pragma once
#include "GfxDriver/CommandBuffer.hpp"

class Camera;
class GizmoContext;
class GizmoRenderer
{
public:
    void SetupDraw(Camera* camera, Gfx::ShaderResource* perScene)
    {
        this->camera = camera;
        this->perScene = perScene;
    }
    void Draw(GizmoContext& context, Gfx::CommandBuffer& cmd);

private:
    Camera* camera;
    Gfx::ShaderResource* perScene;
};
