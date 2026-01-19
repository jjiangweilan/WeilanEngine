#pragma once
#include "RenderCamera.hpp"
#include "StaticMeshInstance.hpp"
#include <memory>

class RenderSceneImpl;
class RenderScene
{
public:
    StaticMeshInstanceHandle CreateStaticMeshInstance();
    void SetProperty(StaticMeshInstanceHandle instance, const StaticMeshInstanceProperty& property);

    RenderCamera CreateRenderCamera();

private:
    std::unique_ptr<RenderSceneImpl> impl;
};
