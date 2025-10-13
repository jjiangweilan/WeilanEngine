#include "Core/Graphics/Mesh.hpp"
#pragma once

namespace Rendering
{
std::unique_ptr<Mesh> GeneratePlane(int width, int height, int vertexCountX = 2, int vertexCountY = 2);
}
