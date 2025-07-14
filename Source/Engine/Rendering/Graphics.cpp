#include "Graphics.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/Material.hpp"
void Graphics::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    GetSingleton().drawCmds.push_back(DrawLineCmd{from, to, color});
}

void Graphics::DrawMesh(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material)
{
    GetSingleton().drawCmds.push_back(
        DrawMeshCmd{mesh.GetSRef<Mesh>(), material.GetSRef<Material>(), model, submeshIndex}
    );
}

void Graphics::DrawTriangle(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color)
{
    GetSingleton().drawCmds.push_back(DrawTriangleCmd{v0, v1, v2, color});
}

void Graphics::DrawCapsule(
    float halfHeight, float radius, const glm::vec3& pos, const glm::quat& rotation, const glm::vec3& scale
)
{
    GetSingleton().drawCmds.push_back(DrawCapsuleCmd{halfHeight, radius, pos, rotation, scale});
}

void Graphics::AddRenderingEvent(
    std::string_view name,
    RenderingEvent event,
    std::function<void(Gfx::CommandBuffer&, const Rendering::RenderingData& renderingData)>&& f
)
{
    GetSingleton().renderingEvents[(int)event].push_back(std::move(f));
}

void Graphics::DrawFrustum(const glm::mat4& viewProj)
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

    auto& s = GetSingleton();
    s.DrawLine(frustumCorners[0], frustumCorners[1], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[1], frustumCorners[2], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[2], frustumCorners[3], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[3], frustumCorners[0], {1, 1, 1, 1});

    s.DrawLine(frustumCorners[3], frustumCorners[7], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[0], frustumCorners[4], {1, 1, 1, 1});

    s.DrawLine(frustumCorners[2], frustumCorners[6], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[1], frustumCorners[5], {1, 1, 1, 1});

    s.DrawLine(frustumCorners[4], frustumCorners[5], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[5], frustumCorners[6], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[6], frustumCorners[7], {1, 1, 1, 1});
    s.DrawLine(frustumCorners[7], frustumCorners[4], {1, 1, 1, 1});
}

Graphics& Graphics::GetSingleton()
{
    static Graphics graphics;
    return graphics;
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
                else if constexpr (std::is_same_v<T, DrawCapsuleCmd>)
                {
                    DrawCapsuleCommand(cmd, draw);
                }
                else if constexpr (std::is_same_v<T, DrawCapsuleCmd>)
                {
                    DrawCmdCommand(cmd, draw);
                }
            },
            drawCmd
        );
    }
}

void Graphics::ClearDraws()
{
    drawCmds.clear();
    for (auto& events : renderingEvents)
    {
        events.clear();
    }
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
            cmd.BindShaderProgram(shader, mat->GetShaderConfig());
            cmd.BindResource(mat->GetSet(Gfx::DescriptorSetSemantics::Material), mat->GetShaderResource());
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

    Gfx::ShaderProgram* lineShaderProgram = EngineInternalResources::GetLineShader().GetShaderProgram();
    cmd.SetPushConstant(lineShaderProgram, (void*)&data);
    cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultShaderConfig());
    cmd.Draw(2, 1, 0, 0);
}

void Graphics::DrawCmdCommand(Gfx::CommandBuffer& cmd, DrawCustomCmd& draw) {}

void Graphics::DrawCapsuleCommand(Gfx::CommandBuffer& cmd, DrawCapsuleCmd& draw)
{
    Submesh* halfSphere = EngineInternalResources::GetHalfSphereMesh();
    Submesh* cylinder = EngineInternalResources::GetCylinderMesh();
    Material* mat = EngineInternalResources::GetDefaultMaterial();
    auto program = mat->GetShader()->GetShaderProgram();

    glm::mat4 cylinderMatrix =
        glm::translate(glm::mat4(1), draw.pos) * glm::mat4_cast(draw.rotation) *
        glm::scale(glm::mat4(1), draw.scale * glm::vec3(draw.radius, draw.halfHeight, draw.radius));

    cmd.BindResource(2, mat->GetShaderResource());

    // Draw cylinder
    cmd.BindIndexBuffer(cylinder->GetIndexBuffer(), 0, cylinder->GetIndexBufferType());
    cmd.BindVertexBuffer(cylinder->GetGfxVertexBufferBindings(), 0);
    cmd.SetPushConstant(program, &cylinderMatrix);
    const Gfx::PipelineConfig& config = mat->GetShaderConfig();
    if (config->polygonMode != Gfx::PolygonMode::Line)
    {
        auto config = *mat->GetShaderConfig();
        cmd.BindShaderProgram(program, config);
    }
    else
    {
        cmd.BindShaderProgram(program, config);
    }
    cmd.DrawIndexed(cylinder->GetIndexCount(), 1, 0, 0, 0);

    // Draw top and bottom half spheres
    float yOffset = draw.halfHeight;
    glm::mat4 halfSphereMatrix0 =
        glm::translate(glm::mat4(1), draw.pos + glm::vec3(0, yOffset, 0)) *
        glm::mat4_cast(draw.rotation) *
        glm::scale(glm::mat4(1), draw.scale * glm::vec3(draw.radius, draw.radius, draw.radius));

    glm::mat4 halfSphereMatrix1 =
        glm::translate(glm::mat4(1), draw.pos + glm::vec3(0, -yOffset, 0)) *
        glm::mat4_cast(draw.rotation) *
        glm::mat4_cast(glm::quat(glm::vec3(glm::radians(180.f), 0, 0))) *
        glm::scale(glm::mat4(1), draw.scale * glm::vec3(draw.radius, draw.radius, draw.radius));

    cmd.BindIndexBuffer(halfSphere->GetIndexBuffer(), 0, halfSphere->GetIndexBufferType());
    cmd.BindVertexBuffer(halfSphere->GetGfxVertexBufferBindings(), 0);

    cmd.SetPushConstant(program, &halfSphereMatrix0);
    cmd.DrawIndexed(halfSphere->GetIndexCount(), 1, 0, 0, 0);

    cmd.SetPushConstant(program, &halfSphereMatrix1);
    cmd.DrawIndexed(halfSphere->GetIndexCount(), 1, 0, 0, 0);
}

void Graphics::DrawTriangleCommand(Gfx::CommandBuffer& cmd, DrawTriangleCmd& draw)
{
    struct
    {
        glm::vec4 v0, v1, v2;
        glm::vec4 color;
    } data;

    data.v0 = glm::vec4(draw.v0, 1.0f);
    data.v1 = glm::vec4(draw.v1, 1.0f);
    data.v2 = glm::vec4(draw.v2, 1.0f);
    data.color = draw.color;

    Gfx::ShaderProgram* triangleShaderProgram = EngineInternalResources::GetLineShader().GetShaderProgram();
    cmd.SetPushConstant(triangleShaderProgram, (void*)&data);
    cmd.BindShaderProgram(triangleShaderProgram, triangleShaderProgram->GetDefaultShaderConfig());
    cmd.Draw(3, 1, 0, 0);
}

Graphics::Graphics()
{
    renderingEvents.resize((int)RenderingEvent::MAX_RENDERING_EVENT);
}

void Graphics::ExecuteRenderingEvnet(
    RenderingEvent event, Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData
)
{
    for (auto& f : renderingEvents[(int)event])
    {
        f(cmd, renderingData);
    }
}
