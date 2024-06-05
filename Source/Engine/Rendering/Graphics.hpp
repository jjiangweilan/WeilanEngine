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

    using DrawCmds = std::variant<DrawLineCmd, DrawMeshCmd>;

    std::vector<DrawCmds> drawCmds;

    void DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    void DrawMeshImpl(Mesh& mesh, int submeshIndex, const glm::mat4& model, Material& material);

    static void DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine);
    static void DrawMeshCommand(Gfx::CommandBuffer& cmd, DrawMeshCmd& drawMesh);
};
