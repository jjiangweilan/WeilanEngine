#include "Graphics.hpp"
#include "EngineInternalShaders.hpp"
#include "GfxDriver/CommandBuffer.hpp"
void Graphics::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
{
    GetSingleton().DrawLineImpl(from, to, color);
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
            },
            drawCmd
        );
    }
}

void Graphics::ClearDraws()
{
    drawCmds.clear();
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

    Gfx::ShaderProgram* lineShaderProgram = EngineInternalShaders::GetLineShader()->GetDefaultShaderProgram();
    cmd.SetPushConstant(lineShaderProgram, (void*)&data);
    cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultShaderConfig());
    cmd.Draw(4, 1, 0, 0);
}
