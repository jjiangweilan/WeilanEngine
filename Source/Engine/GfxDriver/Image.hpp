#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include "ImageDescription.hpp"
#include <cinttypes>
#include <string>

namespace Engine::Gfx
{

// this is from vulkan specs
inline const uint32_t Remaining_Mip_Levels = (~0U);
inline const uint32_t Remaining_Array_Layers = (~0U);

struct ImageSubresourceRange
{
    ImageAspectFlags aspectMask = ImageAspectFlags::Color;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
};

struct ImageSubresourceLayers
{
    ImageAspectFlags aspectMask = ImageAspectFlags::Color;
    uint32_t mipLevel = Remaining_Mip_Levels;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = Remaining_Array_Layers;
};

class Image
{
public:
    virtual ~Image(){};
    virtual void SetName(std::string_view name) = 0;
    virtual const ImageDescription& GetDescription() = 0;
    virtual ImageSubresourceRange GetSubresourceRange() = 0;

protected:
};
} // namespace Engine::Gfx
