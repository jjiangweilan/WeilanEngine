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
        Gfx::ImageIdentifier& srcDepth, Gfx::ImageIdentifier& dstDepth, Gfx::RenderImageDescriptor& dstDepthDesc
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
    Gfx::ImageIdentifier srcDepth;
    Gfx::ImageIdentifier dstDepth;
    Gfx::RenderImageDescriptor dstDepthDesc;
};
} // namespace Rendering::Passes
