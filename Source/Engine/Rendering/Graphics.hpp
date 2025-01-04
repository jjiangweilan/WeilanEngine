#pragma once
#include "Core/SafeReferenceable.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Libs/Math.hpp"
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
    static void DrawCube(
        const glm::float3& positin, const glm::float3& exent, const glm::mat4& model, Material& material
    );
    static void DrawCapsule(
        float height, float radius, const glm::vec3& pos, const glm::quat& rotation, const glm::vec3& scale
    );
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

    struct DrawCapsuleCmd
    {
        float halfHeight;
        float radius;
        glm::vec3 pos;
        glm::quat rotation;
        glm::vec3 scale;
    };

    using DrawCmds = std::variant<DrawLineCmd, DrawMeshCmd, DrawTriangleCmd, DrawCapsuleCmd>;

    std::vector<DrawCmds> drawCmds;

    static void DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& draw);
    static void DrawMeshCommand(Gfx::CommandBuffer& cmd, DrawMeshCmd& draw);
    static void DrawTriangleCommand(Gfx::CommandBuffer& cmd, DrawTriangleCmd& draw);
    static void DrawCapsuleCommand(Gfx::CommandBuffer& cmd, DrawCapsuleCmd& draw);
};
