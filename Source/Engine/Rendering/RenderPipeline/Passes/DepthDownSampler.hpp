#pragma once
#include "Rendering/RenderingData.hpp"
#include "Rendering/Shader2.hpp"

namespace Rendering::Passes
{
class DepthDownSampler
{
public:
    DepthDownSampler();

    void Setup(
        Gfx::RG::ImageIdentifier& srcDepth, Gfx::RG::ImageIdentifier& dstDepth, Gfx::RG::RenderImageDescriptor& dstDepthDesc
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
    Gfx::RG::RenderImageDescriptor dstDepthDesc;
};
} // namespace Rendering::Passes
