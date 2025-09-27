#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "Rendering/RenderingData.hpp"

class ReflectionProbe;

namespace Rendering::Passes
{
class ReflectionProbeUpdate
{
public:
    ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe);

private:
    uint32_t reflectionProbeSize = 256;
    Gfx::ImageIdentifier mainColor;
    Gfx::ImageIdentifier albedo;
    Gfx::ImageIdentifier normal;
    Gfx::ImageIdentifier mask;
    Gfx::ImageIdentifier depth;
    Gfx::RenderImageDescriptor mainColorDescription;
    Gfx::RenderImageDescriptor albedoImageDescription;
    Gfx::RenderImageDescriptor normalImageDescription;
    Gfx::RenderImageDescriptor maskImageDescription;
    Gfx::RenderImageDescriptor depthImageDescription;

    Gfx::RenderPass gbufferPass{};
    std::unique_ptr<Gfx::Buffer> faceBuffers[6];
    std::unique_ptr<Gfx::ShaderResource> faceResources[6];
    std::unique_ptr<Gfx::Image> updatingFaces[6];
};
} // namespace Rendering::Passes
