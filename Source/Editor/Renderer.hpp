#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include <tuple>

namespace Editor
{
class Renderer
{
public:
    // customFont: a path to a font file on disk
    Renderer(Gfx::Image* finalImage, Gfx::Image* fontImage);
    ~Renderer();
    void Execute(ImDrawData* drawData, Gfx::CommandBuffer& cmd);

private:
    std::unique_ptr<Gfx::Buffer> indexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> vertexBuffer = nullptr;
    std::vector<ImDrawVert> vtxDst;
    std::vector<ImDrawIdx> idxDst;
    ObjPtr<Shader> shader = nullptr;
    Gfx::Image* fontImage = nullptr;
    Gfx::Image* finalImage = nullptr;
    ImDrawData* drawData;

    std::unordered_map<UUID, std::unique_ptr<Gfx::ShaderResource>> imageViewToResource;

    Gfx::RenderPass mainPass = Gfx::RenderPass::SingleColor("Editor Pass");

    void RenderEditor(Gfx::CommandBuffer& cmd);

    void BindTexture(Gfx::CommandBuffer& cmd, Gfx::ImageView* imageView);

    VertexAttributes GetFixedVertexAttributes()
    {
        VertexAttributes v;
        v.AddAttribute("position", VertexAttributeSemantics::Position, 0, 8);
        v.AddAttribute("uv", VertexAttributeSemantics::Texcoord, 0, 8);
        v.AddAttribute("color", VertexAttributeSemantics::Color, 0, 4); // R8G8B8A8_UNorm
        return v;
    }
};

} // namespace Editor
