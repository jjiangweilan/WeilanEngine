#pragma once
#include "Runtime/Object/Graphics/Mesh.hpp"
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Runtime/System/Rendering/Material.hpp"

namespace Rendering
{
void DrawMesh(Gfx::CommandBuffer& cmd, Mesh& mesh, Material& material, const float4x4& transform, int materialSet = -1);
}
