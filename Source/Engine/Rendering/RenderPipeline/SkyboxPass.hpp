#pragma once

#include "Core/EngineInternalResources.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"

namespace Rendering::RenderPasses
{
struct SkyboxPass : public RenderPipelinePass
{
    Submesh* cube;
    ObjPtr<Shader> skyboxShader;

    SkyboxPass()
    {
        cube = EngineInternalResources::GetCubeMesh();
        skyboxShader = ShaderLibrary::GetShader(Shaders::Skybox);
    }

    void Execute(Gfx::CommandBuffer* cmd)
    {
        cmd->BeginLabel("Skybox", {0.1, 0.234, 0.674, 1.0});
        cmd->BindVertexBuffer(cube->GetGfxVertexBufferBindings(), 0);
        cmd->BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
        cmd->BindShaderProgram(
            skyboxShader->GetShaderProgram(),
            skyboxShader->GetShaderProgram()->GetDefaultShaderConfig()
        );
        cmd->DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
        cmd->EndLabel();
    }
};

} // namespace Rendering::RenderPasses
