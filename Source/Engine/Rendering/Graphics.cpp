#include "Graphics.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/Material.hpp"
void Graphics::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    GetSingleton().DrawLineImpl(from, to, color);
}

void Graphics::DrawMesh(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material)
{
    GetSingleton().DrawMeshImpl(mesh, submeshIndex, model, material);
}

void Graphics::DrawTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color)
{
    GetSingleton().DrawTriangleImpl(v0, v1, v2, color);
}

void Graphics::DrawFrustum(const glm::mat4& viewProj)
{
    GetSingleton().DrawFrustumImpl(viewProj);
}

void Graphics::DrawMeshImpl(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material)
{
    drawCmds.push_back(DrawMeshCmd{mesh.GetSRef<Mesh>(), material.GetSRef<Material>(), model, submeshIndex});
}

void Graphics::DrawTriangleImpl(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color)
{
    drawCmds.push_back(DrawTriangleCmd{v0, v1, v2, color});
}

Graphics& Graphics::GetSingleton()
{
    static Graphics graphics;
    return graphics;
}

void Graphics::DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    drawCmds.push_back(DrawLineCmd{from, to, color});
}

void Graphics::DrawFrustumImpl(const glm::mat4& viewProj)
{
    std::array<glm::vec4, 8> frustumCorners = {
        glm::vec4(-1, -1, 0, 1),
        glm::vec4(1, -1, 0, 1),
        glm::vec4(1, 1, 0, 1),
        glm::vec4(-1, 1, 0, 1),
        glm::vec4(-1, -1, 1, 1),
        glm::vec4(1, -1, 1, 1),
        glm::vec4(1, 1, 1, 1),
        glm::vec4(-1, 1, 1, 1)
    };

    auto invViewProj = glm::inverse(viewProj);
    for (auto& v : frustumCorners)
    {
        v = invViewProj * v;
        v /= v.w;
    }

    DrawLineImpl(frustumCorners[0], frustumCorners[1], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[1], frustumCorners[2], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[2], frustumCorners[3], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[3], frustumCorners[0], {1, 1, 1, 1});

    DrawLineImpl(frustumCorners[3], frustumCorners[7], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[0], frustumCorners[4], {1, 1, 1, 1});

    DrawLineImpl(frustumCorners[2], frustumCorners[6], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[1], frustumCorners[5], {1, 1, 1, 1});

    DrawLineImpl(frustumCorners[4], frustumCorners[5], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[5], frustumCorners[6], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[6], frustumCorners[7], {1, 1, 1, 1});
    DrawLineImpl(frustumCorners[7], frustumCorners[4], {1, 1, 1, 1});
}

void Graphics::DispatchDraws(Gfx::CommandBuffer& cmd)
{
    for (auto& drawCmd : drawCmds)
    {
        std::visit(
            [&cmd](auto&& draw)
            {
                using T = std::decay_t<decltype(draw)>;
                if constexpr (std::is_same_v<T, DrawLineCmd>)
                {
                    DrawLineCommand(cmd, draw);
                }
                else if constexpr (std::is_same_v<T, DrawMeshCmd>)
                {
                    DrawMeshCommand(cmd, draw);
                }
                else if constexpr (std::is_same_v<T, DrawTriangleCmd>)
                {
                    DrawTriangleCommand(cmd, draw);
                }
            },
            drawCmd
        );
    }
}

void Graphics::ClearDraws()
{
    drawCmds.clear();
}

void Graphics::DrawMeshCommand(Gfx::CommandBuffer& cmd, DrawMeshCmd& drawMesh)
{
    Mesh* mesh = drawMesh.mesh.Get();
    Material* mat = drawMesh.material.Get();

    if (mesh && mat)
    {
        Submesh* submesh = mesh->GetSubmesh(drawMesh.submeshIndex);
        Gfx::ShaderProgram* shader = mat->GetShaderProgram();
        if (submesh && shader)
        {
            auto bindings = submesh->GetGfxVertexBufferBindings();
            cmd.BindIndexBuffer(submesh->GetIndexBuffer(), 0, submesh->GetIndexBufferType());
            cmd.BindVertexBuffer(bindings, 0);
            cmd.SetPushConstant(shader, &drawMesh.model);
            cmd.BindShaderProgram(shader, shader->GetDefaultShaderConfig());
            cmd.BindResource(2, mat->GetShaderResource());
            cmd.DrawIndexed(submesh->GetIndexCount(), 1, 0, 0, 0);
        }
    }
}

void Graphics::DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine)
{
    struct
    {
        glm::vec4 fromPos, toPos;
        glm::vec4 color;
    } data;
    data.fromPos = glm::vec4(drawLine.from, 1.0f);
    data.toPos = glm::vec4(drawLine.to, 1.0f);
    data.color = drawLine.color;

    Gfx::ShaderProgram* lineShaderProgram = EngineInternalResources::GetLineShader()->GetDefaultShaderProgram();
    cmd.SetPushConstant(lineShaderProgram, (void*)&data);
    cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultShaderConfig());
    cmd.Draw(2, 1, 0, 0);
}

void Graphics::DrawTriangleCommand(Gfx::CommandBuffer& cmd, DrawTriangleCmd& drawTriangle)
{
    struct
    {
        glm::vec4 v0, v1, v2;
        glm::vec4 color;
    } data;

    data.v0 = glm::vec4(drawTriangle.v0, 1.0f);
    data.v1 = glm::vec4(drawTriangle.v1, 1.0f);
    data.v2 = glm::vec4(drawTriangle.v2, 1.0f);
    data.color = drawTriangle.color;

    Gfx::ShaderProgram* triangleShaderProgram = EngineInternalResources::GetLineShader()->GetDefaultShaderProgram();
    cmd.SetPushConstant(triangleShaderProgram, (void*)&data);
    cmd.BindShaderProgram(triangleShaderProgram, triangleShaderProgram->GetDefaultShaderConfig());
    cmd.Draw(3, 1, 0, 0);
}
