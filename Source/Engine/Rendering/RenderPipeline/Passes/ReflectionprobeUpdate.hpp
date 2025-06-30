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
    Gfx::RG::ImageIdentifier mainColor;
    Gfx::RG::ImageIdentifier albedo;
    Gfx::RG::ImageIdentifier normal;
    Gfx::RG::ImageIdentifier mask;
    Gfx::RG::ImageIdentifier depth;
    Gfx::RG::ImageDescription mainColorDescription;
    Gfx::RG::ImageDescription albedoImageDescription;
    Gfx::RG::ImageDescription normalImageDescription;
    Gfx::RG::ImageDescription maskImageDescription;
    Gfx::RG::ImageDescription depthImageDescription;

    Gfx::RG::RenderPass gbufferPass{};
    std::unique_ptr<Gfx::Buffer> faceBuffers[6];
    std::unique_ptr<Gfx::ShaderResource> faceResources[6];
    std::unique_ptr<Gfx::Image> updatingFaces[6];
};
} // namespace Rendering::Passes
