#include "RenderTarget.hpp"

namespace Engine
{
RenderTargetAttachmentDescription::RenderTargetAttachmentDescription()
    : format(Gfx::ImageFormat::R16G16B16A16_SFloat), multiSampling(Gfx::MultiSampling::Sample_Count_1),
      mipLevels(1), clearValue{{0, 0, 0, 0}}
{}

RenderTarget::RenderTarget() {}

const RenderTargetDescription& RenderTarget::GetRenderTargetDescription() { return rtDescription; }

bool RenderPassAttachmentConfig::operator==(const RenderPassAttachmentConfig& other) const
{
    return loadOp == other.loadOp && storeOp == other.storeOp && stencilLoadOp == other.stencilLoadOp &&
           stencilStoreOp == other.stencilStoreOp;
}

bool RenderPassConfig::operator==(const RenderPassConfig& other) const
{
    return colors == other.colors && depthStencil == other.depthStencil;
}
} // namespace Engine
