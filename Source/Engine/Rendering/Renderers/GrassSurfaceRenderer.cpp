#include "GrassSurfaceRenderer.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"

GrassSurfaceRenderer::GrassSurfaceRenderer()
{
    grassMesh =
        ((Model*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Models/GrassBlade.glb"))->GetMeshes()[0].get();
}

void GrassSurfaceRenderer::DispatchCompute(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd)
{
    // dispatch
    // cmd.BindResource(2, grassSurface.computeDispatchMat.GetShaderResource());
    // cmd.BindShaderProgram(
    //     grassSurface.computeDispatchMat.GetShaderProgram(),
    //     grassSurface.computeDispatchMat.GetShaderProgram()->GetDefaultShaderConfig()
    // );
    // cmd.Dispatch(1, 1, 1);
}
void GrassSurfaceRenderer::Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd)
{

    // draw
    auto submesh = grassMesh->GetSubmesh(0);
    cmd.BindVertexBuffer(submesh->GetGfxVertexBufferBindings(), 0);
    cmd.BindIndexBuffer(submesh->GetIndexBuffer(), 0, submesh->GetIndexBufferType());
    cmd.BindResource(2, grassSurface.drawMat.GetShaderResource());
    glm::vec4 pos = glm::vec4(grassSurface.GetGameObject()->GetPosition(), 1.0);
    cmd.SetPushConstant(grassSurface.drawMat.GetShaderProgram(), &pos);
    cmd.BindShaderProgram(
        grassSurface.drawMat.GetShaderProgram(),
        grassSurface.drawMat.GetShaderProgram()->GetDefaultShaderConfig()
    );
    cmd.DrawIndexed(submesh->GetIndexCount(), 1024, 0, 0, 0);
}
