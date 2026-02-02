#include "GrassModule.hpp"
#include "Engine/Runtime/System/Rendering/CommandBufferUtils.hpp"

namespace Rendering
{

void GrassModule::AddGrass(float4 position)
{
    grassPositions.push_back(position);
}

void GrassModule::BeforeRenderSceneUpdate()
{
}

void GrassModule::Render(Gfx::CommandBuffer& cmd)
{
    Gfx::ShaderProgram* shaderProgram = grassShader->GetShaderProgram();
    Mesh* mesh = grassCluser;

    if (shaderProgram && mesh)
    {
        Rendering::DrawMesh(cmd, *mesh, *shaderProgram);
    }
}

} // namespace Rendering
