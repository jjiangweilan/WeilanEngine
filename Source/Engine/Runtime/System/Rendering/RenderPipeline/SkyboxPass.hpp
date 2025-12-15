#pragma once

#include "MiddleLayer/EngineInternalResources.hpp"
#include "Runtime/Object/Graphics/Mesh.hpp"
#include "Runtime/System/Rendering/Shader.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"

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
