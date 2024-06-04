#pragma once
#include <glm/glm.hpp>
#include <variant>
#include <vector>

namespace Gfx
{
class CommandBuffer;
}

class Graphics
{
public:
    static void DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color = {1, 1, 1, 1});

    static Graphics& GetSingleton();

    void DispatchDraws(Gfx::CommandBuffer& cmd);
    void ClearDraws();

private:
    struct DrawLineCmd
    {
        glm::vec3 from, to;
        glm::vec4 color;
    };

    using DrawCmds = std::variant<DrawLineCmd>;

    std::vector<DrawCmds> drawCmds;

    void DrawLineImpl(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    static void DrawLineCommand(Gfx::CommandBuffer& cmd, DrawLineCmd& drawLine);
};
