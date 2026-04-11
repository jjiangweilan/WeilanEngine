#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"

namespace Rendering::Passes
{
class PixelZoomPass : public RenderPipelinePass
{
public:
    PixelZoomPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& src,
        const Gfx::ImageIdentifier& dst,
        glm::vec2 mousePos,
        glm::vec2 screenSize
    );

    const Gfx::ImageIdentifier& GetOutputId() const { return outputId; }

private:
    Gfx::RenderPass pass = Gfx::RenderPass(1, 1);
    Gfx::ImageIdentifier outputId = "PixelZoom";
    ObjPtr<Shader> shader;
    std::unique_ptr<Gfx::ShaderResource> resource;

    static constexpr float zoomFactor = 4.0f;
    static constexpr float zoomWindowSize = 100.0f; // half-size in pixels
};
} // namespace Rendering::Passes
