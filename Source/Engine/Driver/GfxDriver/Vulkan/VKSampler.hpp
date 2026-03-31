#pragma once
#include "../Sampler.hpp"
#include "VKCommon.hpp"

namespace Gfx
{

class VKSampler : public Sampler
{
    DECLARE_OBJECT();

public:
    VKSampler() = default;
    VKSampler(const CreateInfo& createInfo);
    VKSampler(const VKSampler&) = delete;
    ~VKSampler() override = default;

    VkSampler GetVkSampler() const { return sampler; }

private:
    VkSampler sampler = VK_NULL_HANDLE;
};

} // namespace Gfx
