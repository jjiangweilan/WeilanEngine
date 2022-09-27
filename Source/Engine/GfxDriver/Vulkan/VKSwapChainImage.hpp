#pragma once
#include "VKImage.hpp"

namespace Engine::Gfx
{
    class VKSwapChainImage : public VKImage
    {
    public:
        VKSwapChainImage(RefPtr<VKContext> context, VkImage image, VkFormat format, uint32_t width, uint32_t height);
        VKSwapChainImage(VKSwapChainImage&& other) : VKImage(std::move(other)){};
        VKSwapChainImage() = default;
        ~VKSwapChainImage() override;
    };
}