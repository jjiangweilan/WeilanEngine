#pragma once
#include "RenderCamera.hpp"
#include "StaticMeshRenderer.hpp"
#include <memory>

class RenderSceneImpl;
class RenderScene
{
public:
    StaticMeshRenderer CreateStaticMeshRenderer();
    void DestroyStaticMeshRenderer(StaticMeshRenderer& renderer);

    RenderCamera CreateRenderCamera();

private:
    std::unique_ptr<RenderSceneImpl> impl;
};
