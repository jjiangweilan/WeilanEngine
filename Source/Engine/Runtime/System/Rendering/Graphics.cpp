#include "Graphics.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

namespace
{
struct LineData
{
    glm::vec4 fromPos, toPos;
    glm::vec4 color;
};
}

void Graphics::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    DrawLines(std::vector<Line>{{from, to, color}});
}

void Graphics::DrawLines(const std::vector<Line>& lines)
{
    if (lines.empty())
    {
        return;
    }

    GetSingleton().drawCmds.push_back(DrawLineCmd{lines});
}

void Graphics::DrawMesh(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material)
{
    GetSingleton().drawCmds.push_back(DrawMeshCmd{&mesh, &material, model, submeshIndex});
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

void Graphics::DrawCube(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rotation)
{
    GetSingleton().drawCmds.push_back(DrawCubeCmd{pos, scale, rotation});
}

void Graphics::DrawSphere(const glm::vec3& pos, const glm::vec3& scale)
{
    Mesh* sphere = EngineInternalResources::GetModels().sphere;
    Material* mat = EngineInternalResources::GetDefaultMaterial();
    if (!sphere || !mat)
    {
        return;
    }

    glm::mat4 m = glm::translate(glm::mat4(1), pos) * glm::scale(glm::mat4(1), scale);
    DrawMesh(*sphere, 0, m, *mat);
}

void Graphics::DrawPlane(const glm::vec3& normal, float w)
{
    GetSingleton().drawCmds.push_back(DrawPlaneCmd{normal, w});
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
                    DrawLinesCommand(cmd, draw);
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
                else if constexpr (std::is_same_v<T, DrawCustomCmd>)
                {
                    DrawCmdCommand(cmd, draw);
                }
                else if constexpr (std::is_same_v<T, DrawCubeCmd>)
                {
                    DrawCubeCommand(cmd, draw);
                }
                else if constexpr (std::is_same_v<T, DrawPlaneCmd>)
                {
                    DrawPlaneCommand(cmd, draw);
                }
            },
            drawCmd
        );
    }
}

void Graphics::PrepareDraws(Gfx::CommandBuffer& cmd)
{
    for (auto& drawCmd : drawCmds)
    {
        std::visit(
            [&cmd](auto&& draw)
            {
                using T = std::decay_t<decltype(draw)>;
                if constexpr (std::is_same_v<T, DrawLineCmd>)
                {
                    PrepareDrawLines(cmd, draw);
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

void Graphics::PrepareDrawLines(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine)
{
    std::vector<LineData> lines;
    lines.reserve(drawLine.lines.size());
    for (const Line& line : drawLine.lines)
    {
        lines.push_back({glm::vec4(line.from, 1.0f), glm::vec4(line.to, 1.0f), line.color});
    }

    drawLine.lineBuffer = cmd.AllocateBuffer(
        sizeof(LineData) * lines.size(),
        Gfx::TemporaryBufferUsage::Storage,
        alignof(LineData)
    );
    cmd.UploadData(drawLine.lineBuffer, lines.data(), sizeof(LineData) * lines.size());
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
            cmd.BindShaderProgram(shader, mat->GetPipelineConfig());
            cmd.BindResource(mat->GetSet(Gfx::DescriptorSetSemantics::Material), mat->GetShaderResource());
            cmd.DrawIndexed(submesh->GetIndexCount(), 1, 0, 0, 0);
        }
    }
}

void Graphics::DrawLinesCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine)
{
    if (!drawLine.lineBuffer.IsValid())
        return;

    Shader& lineShader = EngineInternalResources::GetLineShader();
    Gfx::ShaderProgram* lineShaderProgram = lineShader.GetShaderProgram();
    int lineSet = lineShader.GetSet(Gfx::DescriptorSetSemantics::Material);

    cmd.BindResource(lineSet, std::vector<Gfx::DynamicBinding>{Gfx::DynamicBinding("lineData", drawLine.lineBuffer)});
    cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultPipelineConfig());
    cmd.Draw(static_cast<uint32_t>(drawLine.lines.size() * 2), 1, 0, 0);
}

void Graphics::DrawCmdCommand(Gfx::CommandBuffer& cmd, DrawCustomCmd& draw) {}

void Graphics::DrawCubeCommand(Gfx::CommandBuffer& cmd, DrawCubeCmd& draw)
{
    Submesh* cube = EngineInternalResources::GetCubeMesh();
    Material* mat = EngineInternalResources::GetDefaultMaterial();
    Gfx::ShaderProgram* program = mat->GetShader()->GetShaderProgram();

    glm::mat4 m =
        glm::translate(glm::mat4(1), draw.pos) * glm::mat4_cast(draw.rotation) * glm::scale(glm::mat4(1), draw.scale);

    cmd.BindResource(2, mat->GetShaderResource());
    cmd.BindIndexBuffer(cube->GetIndexBuffer(), 0, cube->GetIndexBufferType());
    cmd.BindVertexBuffer(cube->GetGfxVertexBufferBindings(), 0);
    cmd.SetPushConstant(program, &m);
    cmd.BindShaderProgram(program, mat->GetPipelineConfig());
    cmd.DrawIndexed(cube->GetIndexCount(), 1, 0, 0, 0);
}

void Graphics::DrawCapsuleCommand(Gfx::CommandBuffer& cmd, DrawCapsuleCmd& draw)
{
    Submesh* halfSphere = EngineInternalResources::GetHalfSphereMesh();
    Submesh* cylinder = EngineInternalResources::GetCylinderMesh();
    Material* mat = EngineInternalResources::GetDefaultMaterial();
    Gfx::ShaderProgram* program = mat->GetShader()->GetShaderProgram();

    glm::mat4 cylinderMatrix =
        glm::translate(glm::mat4(1), draw.pos) * glm::mat4_cast(draw.rotation) *
        glm::scale(glm::mat4(1), draw.scale * glm::vec3(draw.radius, draw.halfHeight, draw.radius));

    cmd.BindResource(2, mat->GetShaderResource());

    // Draw cylinder
    cmd.BindIndexBuffer(cylinder->GetIndexBuffer(), 0, cylinder->GetIndexBufferType());
    cmd.BindVertexBuffer(cylinder->GetGfxVertexBufferBindings(), 0);
    cmd.SetPushConstant(program, &cylinderMatrix);
    const Gfx::PipelineConfig& config = mat->GetPipelineConfig();
    if (config->polygonMode != Gfx::PolygonMode::Line)
    {
        auto config = *mat->GetPipelineConfig();
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
        glm::translate(glm::mat4(1), draw.pos + glm::vec3(0, yOffset, 0)) * glm::mat4_cast(draw.rotation) *
        glm::scale(glm::mat4(1), draw.scale * glm::vec3(draw.radius, draw.radius, draw.radius));

    glm::mat4 halfSphereMatrix1 =
        glm::translate(glm::mat4(1), draw.pos + glm::vec3(0, -yOffset, 0)) * glm::mat4_cast(draw.rotation) *
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

    Gfx::ShaderProgram* triangleShaderProgram = EngineInternalResources::GetTriangleShader().GetShaderProgram();
    cmd.SetPushConstant(triangleShaderProgram, (void*)&data);
    cmd.BindShaderProgram(triangleShaderProgram, triangleShaderProgram->GetDefaultPipelineConfig());
    cmd.Draw(3, 1, 0, 0);
}

void Graphics::DrawPlaneCommand(Gfx::CommandBuffer& cmd, DrawPlaneCmd& draw)
{
    Submesh* planeMesh = EngineInternalResources::GetPlaneMesh();
    Material* mat = EngineInternalResources::GetDefaultMaterial();
    Gfx::ShaderProgram* program = mat->GetShader()->GetShaderProgram();
    static auto GetConfig = []()
    {
        Material* mat = EngineInternalResources::GetDefaultMaterial();
        const Gfx::PipelineConfig& config = mat->GetPipelineConfig();
        auto config_v = *config;
        config_v.cullMode = Gfx::CullMode::None;  // Because the plane is single sided
        return config_v;
    };
    static Gfx::PipelineConfig planeConfig = GetConfig();

    float3 norm = draw.normal;

    // Calculate plane position from normal and distance
    // The plane equation is: normal.x * x + normal.y * y + normal.z * z + w = 0
    // So the closest point on the plane to origin is at distance -w along the normal
    glm::vec3 planePosition = norm * (-draw.w);
    
    // Calculate rotation to align the plane with the normal
    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::normalize(glm::cross(up, norm));
    if (glm::length(right) < 0.001f) // Handle case where normal is parallel to up
    {
        up = glm::vec3(1, 0, 0);
        right = glm::normalize(glm::cross(up, norm));
    }
    up = glm::cross(norm, right);
    
    glm::mat3 horizontalFlip = glm::mat3(
        float3(1, 0, 0),
        float3(0, 0, 1),
        float3(0, -1, 0)
    ); // Because the plane mesh is created in the XZ plane

    glm::mat3 rotationMatrix = glm::mat3(right, up, norm) * horizontalFlip;
    
    // Create transformation matrix
    glm::mat4 transform = glm::translate(glm::mat4(1), planePosition) * float4x4(rotationMatrix);

    cmd.BindResource(2, mat->GetShaderResource());
    cmd.BindIndexBuffer(planeMesh->GetIndexBuffer(), 0, planeMesh->GetIndexBufferType());
    cmd.BindVertexBuffer(planeMesh->GetGfxVertexBufferBindings(), 0);
    cmd.SetPushConstant(program, &transform);
    cmd.BindShaderProgram(program, planeConfig);
    cmd.DrawIndexed(planeMesh->GetIndexCount(), 1, 0, 0, 0);
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
