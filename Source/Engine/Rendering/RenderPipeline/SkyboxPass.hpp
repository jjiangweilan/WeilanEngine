#pragma once

#include "Core/EngineInternalResources.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Rendering/Shader2.hpp"
namespace Rendering::RenderPasses
{
struct SkyboxPass
{
    Submesh* cube;
    ObjPtr<Shader2> skyboxShader;
    Gfx::RG::RenderPass pass = Gfx::RG::RenderPass(1, 2);

    SkyboxPass()
    {
        pass = Gfx::RG::RenderPass::Default(
            "Skybox",
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store
        );

        cube = EngineInternalResources::GetCubeMesh();
        skyboxShader = ShaderLibrary::GetShader(ShaderLibrary::Skybox);
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
