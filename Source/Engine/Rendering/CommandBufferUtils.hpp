#pragma once
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/Material.hpp"

namespace Rendering
{
void DrawMesh(Gfx::CommandBuffer& cmd, Mesh& mesh, Material& material, const float4x4& transform, int materialSet = -1);
}
