#include "CommandBufferUtils.hpp"

namespace Rendering
{
void DrawMesh(Gfx::CommandBuffer& cmd, Mesh& mesh, Material& material, const float4x4& transform, int materialSet)
{
    auto materialResource = material.GetShaderResource();
    auto shaderProgram = material.GetShaderProgram();

    for (auto& submesh : mesh.GetSubmeshes())
    {
        auto shader = material.GetShader();
        if (shader != nullptr)
        {
            cmd.BindVertexBuffer(submesh.GetGfxVertexBufferBindings(), 0);
            cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
            if (materialSet >= 0)
                cmd.BindResource(materialSet, materialResource);
            cmd.BindShaderProgram(shaderProgram, material.GetShaderConfig());
            cmd.SetPushConstant(shaderProgram, (void*)&transform);
            cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);
        }
    }
}
} // namespace Rendering
