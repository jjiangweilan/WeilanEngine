#pragma once
#include "Core/Graphics/Mesh.hpp"
#include "Core/Ptr.hpp"

#include "GfxDriver/Buffer.hpp"
#include "Modules/Ocean/OceanQuadTree.hpp"
#include "Rendering/Shader.hpp"

class OceanRenderer
{
public:
    OceanRenderer();
private:
    std::unique_ptr<Mesh> plane;
};
