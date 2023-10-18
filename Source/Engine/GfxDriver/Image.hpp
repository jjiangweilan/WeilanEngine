#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include "ImageDescription.hpp"
#include <cinttypes>
#include <glm/glm.hpp>
#include <stdexcept>
#include <string>
#include <vector>

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

    std::vector<ImageSubresourceRange> Subtract(const ImageSubresourceRange& other)
    {
        if (aspectMask != other.aspectMask)
            throw std::logic_error("image aspect mask must match");

        // subtract mip
        constexpr uint32_t ZERO = 0;
        int rBaseMipLevelLeftLength = glm::clamp(other.baseMipLevel - baseMipLevel, ZERO, levelCount);
    }

    bool CoversElement(uint32_t mipLevel, uint32_t layer) const
    {
        return mipLevel >= baseMipLevel && mipLevel <= baseMipLevel + levelCount && layer >= baseArrayLayer &&
               layer <= baseArrayLayer + layerCount;
    }

    bool Covers(const ImageSubresourceRange& other) const
    {
        return other.aspectMask == aspectMask && other.baseMipLevel >= baseMipLevel &&
               other.baseMipLevel + other.levelCount <= baseMipLevel + levelCount &&
               other.baseArrayLayer >= baseArrayLayer && other.baseArrayLayer <= baseArrayLayer + layerCount;
    }

    bool operator==(const ImageSubresourceRange& other) const = default;
};

struct ImageSubresourceLayers
{
    ImageAspectFlags aspectMask = ImageAspectFlags::Color;
    uint32_t mipLevel = 0;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = Remaining_Array_Layers;
};

class ImageView;
class Image
{
public:
    virtual ~Image(){};
    virtual void SetName(std::string_view name) = 0;
    virtual const ImageDescription& GetDescription() = 0;
    virtual ImageSubresourceRange GetSubresourceRange() = 0;
    virtual ImageView& GetDefaultImageView() = 0;

protected:
};
} // namespace Engine::Gfx
