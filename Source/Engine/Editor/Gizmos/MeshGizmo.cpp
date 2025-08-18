#include "MeshGizmo.hpp"

void GizmoDrawMesh::Draw(Gfx::CommandBuffer& cmd)
{
    auto& submeshes = mesh->GetSubmeshes();
    if (submeshIndex < submeshes.size())
    {
        auto& submesh = submeshes[submeshIndex];
        cmd.BindResource(0, perScene);
        cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
        cmd.BindVertexBuffer(submesh.GetGfxVertexBufferBindings(), 0);
        if (material != nullptr)
        {
            Gfx::ShaderProgram* program = material->GetShaderProgram();
            cmd.BindResource(material->GetSet(Gfx::DescriptorSetSemantics::Material), material->GetShaderResource());
            cmd.SetPushConstant(program, &modelMatrix);
            cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
        }
        else
        {
            cmd.SetPushConstant(shader->GetShaderProgram(), &modelMatrix);
            cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
        }
        cmd.DrawIndexed(submesh.GetIndexCount(), 1, 0, 0, 0);
    }
}

bool GizmoDrawMesh::Pick(const Ray& ray)
{
    float t;
    return RayVsAABB(ray, GetAABB(), t) && t > 0;
}

AABB GizmoDrawMesh::GetAABB()
{
    auto& submeshes = mesh->GetSubmeshes();
    if (submeshIndex < submeshes.size())
    {
        auto aabb = submeshes[submeshIndex].GetAABB();
        aabb.max += glm::vec3(modelMatrix[3]);
        aabb.min += glm::vec3(modelMatrix[3]);
        return aabb;
    }
    else
        return AABB(glm::vec3(0), glm::vec3(0));
}
