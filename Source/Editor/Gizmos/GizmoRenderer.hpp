#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"

class Camera;
class GizmoManager;
class GizmoRenderer
{
public:
    void SetupDraw(Camera* camera, Gfx::ShaderResource* perScene)
    {
        this->camera = camera;
        this->perScene = perScene;
    }
    void Draw(GizmoManager& gizmoManager, Gfx::CommandBuffer& cmd);

private:
    Camera* camera;
    Gfx::ShaderResource* perScene;
};
