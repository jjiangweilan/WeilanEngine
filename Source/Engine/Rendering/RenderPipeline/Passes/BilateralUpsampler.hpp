#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Shader2.hpp"

namespace Rendering::Passes
{
class BilateralUpsampler
{
public:
    BilateralUpsampler();

    void Setup(
        Gfx::RG::ImageIdentifier& srcDepth, Gfx::RG::ImageIdentifier& dstDepth, Gfx::RG::ImageDescription& dstDepthDesc
    )
    {
        this->srcDepth = srcDepth;
        this->dstDepth = dstDepth;
        this->dstDepthDesc = dstDepthDesc;
    }
    void Execute(Gfx::CommandBuffer& cmd);

private:
    ObjPtr<Shader2> shader;

    Material resource;
    Gfx::RG::ImageIdentifier srcDepth;
    Gfx::RG::ImageIdentifier dstDepth;
    Gfx::RG::ImageDescription dstDepthDesc;
};
} // namespace Rendering::Passes
