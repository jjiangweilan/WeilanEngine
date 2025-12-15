#pragma once
#include "Runtime/System/Rendering/RenderingData.hpp"
#include "Runtime/System/Rendering/Shader.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
namespace Rendering::Passes
{
class DepthDownSampler : public RenderPipelinePass
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
    ObjPtr<Shader> shader;

    Material resource;
    Gfx::ImageIdentifier srcDepth;
    Gfx::ImageIdentifier dstDepth;
    Gfx::RenderImageDescriptor dstDepthDesc;
};
} // namespace Rendering::Passes
