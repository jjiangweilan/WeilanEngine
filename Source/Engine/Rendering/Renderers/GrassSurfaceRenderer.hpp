#pragma once
#include "Core/Component/GrassSurface.hpp"
#include "GfxDriver/CommandBuffer.hpp"

class GrassSurfaceRenderer
{
public:
    GrassSurfaceRenderer();
    void DispatchCompute(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);
    void Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd);

private:
    Mesh* grassMesh;
};
