#pragma once
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"

class GrassSurfaceRenderer
{
public:
    GrassSurfaceRenderer();
    void DispatchCompute(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);
    void Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);

private:
    Mesh* grassMesh;
};
