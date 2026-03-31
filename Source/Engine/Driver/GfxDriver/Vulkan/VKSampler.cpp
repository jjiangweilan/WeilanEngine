#include "VKSampler.hpp"
#include "VKContext.hpp"
#include "VKShaderProgram.hpp"

namespace Gfx
{

DEFINE_OBJECT(Object, VKSampler, "7B506CAD-6ACB-457F-AFB2-5F7E9B860C1A");

VKSampler::VKSampler(const CreateInfo& createInfo)
{
    // Map CreateInfo into a SamplerConfig to reuse SamplerCachePool's creation and deduplication.
    ShaderPipelineInfo::SamplerConfig config{};
    config.addressModeU = createInfo.addressModeU;
    config.addressModeV = createInfo.addressModeV;
    config.addressModeW = createInfo.addressModeW;
    config.mipmapMode = createInfo.mipmapMode;
    config.minFilter = createInfo.minFilter;
    config.magFilter = createInfo.magFilter;
    config.anisotropic = createInfo.anisotropic;
    config.enableCompare = createInfo.enableCompare;

    VkSamplerCreateInfo ci = SamplerCachePool::GenerateSamplerCreateInfo(config);
    sampler = SamplerCachePool::RequestSampler(ci);
    // VkSampler lifetime is managed by SamplerCachePool; no explicit cleanup needed here.
}

} // namespace Gfx
