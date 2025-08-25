#pragma once
#include "Geometry.hpp"

namespace Gfx
{
class CommandBuffer;
};

class GeometryRendering
{
public:
    static void DrawWireBox(Gfx::CommandBuffer& cmd, const Box& box);
};
