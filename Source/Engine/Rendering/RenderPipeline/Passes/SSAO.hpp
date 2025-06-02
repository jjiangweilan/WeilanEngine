#pragma once
#include "Rendering/Material.hpp"
#include "Rendering/RenderingData.hpp"
#include "Rendering/Shader2.hpp"

namespace Rendering::Passes
{
class SSAO
{
public:
    SSAO();
    ObjPtr<Shader2> ssaoShader;
    Material mat;
    Material bilateralMat;
    Gfx::RG::ImageIdentifier ssao = Gfx::RG::ImageIdentifier("SSAO");

    Gfx::RG::RenderPass pass = Gfx::RG::RenderPass::SingleColor("SSAO");
    void Execute(
        Gfx::CommandBuffer* cmd, const Gfx::RG::ImageIdentifier& texDepth, const Gfx::RG::ImageDescription& depthTexDesc, RenderPipelineSetting* setting
    );
};
} // namespace Rendering::Passes
