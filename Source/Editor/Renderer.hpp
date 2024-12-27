#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "Rendering/Shader2.hpp"
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
    std::unique_ptr<Gfx::Buffer> stagingBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> stagingBuffer2 = nullptr;
    ObjPtr<Shader2> shader = nullptr;
    Gfx::Image* fontImage = nullptr;
    Gfx::Image* finalImage = nullptr;
    ImDrawData* drawData;

    std::unordered_map<UUID, std::unique_ptr<Gfx::ShaderResource>> imageViewToResource;

    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::SingleColor("Editor Pass");

    void RenderEditor(Gfx::CommandBuffer& cmd);

    void BindTexture(Gfx::CommandBuffer& cmd, Gfx::ImageView* imageView);
    
};

} // namespace Editor
