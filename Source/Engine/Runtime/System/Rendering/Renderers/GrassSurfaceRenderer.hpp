#pragma once
#include "Runtime/Object/Component/GrassSurface.hpp"
#include "Driver/GfxDriver/CommandBuffer.hpp"

class GrassSurfaceRenderer
{
public:
    GrassSurfaceRenderer();
    void DispatchCompute(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);
    void Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);

private:
    Mesh* grassMesh;
};
