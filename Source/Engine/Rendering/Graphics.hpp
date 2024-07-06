#pragma once
#include "Core/SafeReferenceable.hpp"
#include <glm/glm.hpp>
#include <variant>
#include <vector>

namespace Gfx
{
class CommandBuffer;
}

class Material;
class Mesh;
class Graphics
{
public:
    static void DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color = {1, 1, 1, 1});
    static void DrawMesh(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material);
    static void DrawTriangle(
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color = {1, 1, 1, 1}
    );
    static void DrawFrustum(const glm::mat4& viewProj);

    static Graphics& GetSingleton();

    void DispatchDraws(Gfx::CommandBuffer& cmd);
    void ClearDraws();

private:
    struct DrawLineCmd
    {
        glm::vec3 from, to;
        glm::vec4 color;
    };

    struct DrawMeshCmd
    {
        SRef<Mesh> mesh;
        SRef<Material> material;
        glm::mat4 model;
        int submeshIndex;
    };

    struct DrawTriangleCmd
    {
        glm::vec3 v0;
        glm::vec3 v1;
        glm::vec3 v2;
        glm::vec4 color;
    };

    using DrawCmds = std::variant<DrawLineCmd, DrawMeshCmd, DrawTriangleCmd>;

    std::vector<DrawCmds> drawCmds;

    void DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);

    // using DrwaLineImpl
    void DrawFrustumImpl(const glm::mat4& viewProj);

    void DrawTriangleImpl(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color);
    void DrawMeshImpl(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material);

    static void DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine);
    static void DrawMeshCommand(Gfx::CommandBuffer& cmd, DrawMeshCmd& drawMesh);
    static void DrawTriangleCommand(Gfx::CommandBuffer& cmd, DrawTriangleCmd& drawMesh);
};
