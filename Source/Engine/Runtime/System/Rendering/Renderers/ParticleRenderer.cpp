#include "ParticleRenderer.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"

namespace Rendering
{
ParticleRenderer::ParticleRenderer()
{
    particleShader = ShaderLibrary::GetShader(Shaders::Particle);
    parameterSetIdx = particleShader->GetSet(Gfx::DescriptorSetSemantics::Material);
}
void ParticleRenderer::Draw(Gfx::CommandBuffer& cmd, const ParticleDraw& draw)
{
    if (draw.particleCount > 0 && draw.particleShaderParameters != nullptr && draw.instancingMesh != nullptr)
    {
        auto particleShader = this->particleShader->GetShaderProgram();

        cmd.BindIndexBuffer(draw.instancingMesh->GetIndexBuffer(), 0, draw.instancingMesh->GetIndexBufferType());
        cmd.BindVertexBuffer(draw.instancingMesh->GetGfxVertexBufferBindings(), 0);
        cmd.BindShaderProgram(particleShader, particleShader->GetDefaultPipelineConfig());
        cmd.BindResource(parameterSetIdx, draw.particleShaderParameters);
        cmd.SetPushConstant(particleShader, (void*)&draw.rootTransform[0]);
        cmd.DrawIndexed(draw.instancingMesh->GetIndexCount(), draw.particleCount, 0, 0, 0);
    }
}
} // namespace Rendering
