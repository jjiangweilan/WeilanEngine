#pragma once
#include "Core/SafeReferenceable.hpp"
#include "Core/Math/Geometry.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Libs/DynamicArray.hpp"
#include "Libs/Math.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Structs.hpp"
#include <variant>
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
    static void DrawCube(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rotation);
    static void DrawPlane(const glm::vec3& normal, float w);
    static void DrawCapsule(
        float height, float radius, const glm::vec3& pos, const glm::quat& rotation, const glm::vec3& scale
    );
    static void DrawTriangle(
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec4& color = {1, 1, 1, 1}
    );
    static void DrawFrustum(const glm::mat4& viewProj);
    static void AddRenderingEvent(
        std::string_view name,
        RenderingEvent event,
        std::function<void(Gfx::CommandBuffer&, const Rendering::RenderingData& renderingData)>&& f
    );

    static Graphics& GetSingleton();

    void ExecuteRenderingEvnet(
        RenderingEvent event, Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData
    );

    void DispatchDraws(Gfx::CommandBuffer& cmd);
    void ClearDraws();

private:
    Graphics();
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

    struct DrawCubeCmd
    {
        glm::vec3 pos;
        glm::vec3 scale;
        glm::quat rotation;
    };

    struct DrawPlaneCmd
    {
        glm::vec3 normal;
        float w;
    };

    struct DrawCustomCmd
    {
        DrawCustomCmd(
            RenderingEvent e,
            std::function<void(Gfx::CommandBuffer&, const Rendering::RenderingData& renderingData)>&& f
        )
            : event(e), f(std::move(f))
        {}
        RenderingEvent event;
        std::function<void(Gfx::CommandBuffer&, const Rendering::RenderingData& renderingData)> f;
    };

    using DrawCmds =
        std::variant<DrawLineCmd, DrawMeshCmd, DrawTriangleCmd, DrawCapsuleCmd, DrawCubeCmd, DrawPlaneCmd, DrawCustomCmd>;

    DynamicArray<DrawCmds> drawCmds;
    DynamicArray<DynamicArray<std::function<void(Gfx::CommandBuffer&, const Rendering::RenderingData& renderingData)>>>
        renderingEvents;

    static void DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& draw);
    static void DrawMeshCommand(Gfx::CommandBuffer& cmd, DrawMeshCmd& draw);
    static void DrawTriangleCommand(Gfx::CommandBuffer& cmd, DrawTriangleCmd& draw);
    static void DrawCapsuleCommand(Gfx::CommandBuffer& cmd, DrawCapsuleCmd& draw);
    static void DrawCmdCommand(Gfx::CommandBuffer& cmd, DrawCustomCmd& draw);
    static void DrawCubeCommand(Gfx::CommandBuffer& cmd, DrawCubeCmd& draw);
    static void DrawPlaneCommand(Gfx::CommandBuffer& cmd, DrawPlaneCmd& draw);
};
