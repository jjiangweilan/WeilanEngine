#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include "Core/SafeReferenceable.hpp"
#include "ImageDescription.hpp"
#include <cinttypes>
#include <glm/glm.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace Gfx
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
        int rBaseMipLevelLeftCount = glm::clamp(other.baseMipLevel - baseMipLevel, 0u, levelCount);
        int rBaseMipLevelRightCount =
            glm::clamp(baseMipLevel + levelCount - other.baseMipLevel - other.levelCount, 0u, levelCount);

        int rBaseLayerLeftCount = glm::clamp(other.baseArrayLayer - baseArrayLayer, 0u, layerCount);
        int rBaseLayerRightCount =
            glm::clamp(baseArrayLayer + layerCount - other.baseArrayLayer - other.layerCount, 0u, layerCount);

        std::vector<ImageSubresourceRange> rtn{};
        if (rBaseMipLevelLeftCount != 0 && rBaseLayerLeftCount != 0)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                baseMipLevel,
                (uint32_t)rBaseMipLevelLeftCount,
                baseArrayLayer,
                (uint32_t)rBaseLayerLeftCount
            });
        }

        if (rBaseMipLevelLeftCount != 0 && rBaseLayerRightCount != 0)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                baseMipLevel,
                (uint32_t)rBaseMipLevelLeftCount,
                other.baseArrayLayer + other.layerCount,
                (uint32_t)rBaseLayerRightCount,
            });
        }

        if (rBaseMipLevelRightCount != 0 && rBaseLayerLeftCount != 0)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                other.baseMipLevel + other.levelCount,
                (uint32_t)rBaseMipLevelRightCount,
                baseArrayLayer,
                (uint32_t)rBaseLayerLeftCount,
            });
        }

        if (rBaseMipLevelRightCount != 0 && rBaseLayerRightCount != 0)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                other.baseMipLevel + other.levelCount,
                (uint32_t)rBaseMipLevelRightCount,
                other.baseArrayLayer + other.layerCount,
                (uint32_t)rBaseLayerRightCount,
            });
        }

        return rtn;
    }

    bool CoversElement(uint32_t mipLevel, uint32_t layer) const
    {
        return mipLevel >= baseMipLevel && mipLevel <= baseMipLevel + levelCount && layer >= baseArrayLayer &&
               layer <= baseArrayLayer + layerCount;
    }

    ImageSubresourceRange And(const ImageSubresourceRange& other)
    {
        if (aspectMask != other.aspectMask)
            throw std::logic_error("image aspect mask must match");

        uint32_t lm = glm::max(baseMipLevel, other.baseMipLevel);
        uint32_t rm = glm::min(baseMipLevel + levelCount, other.baseMipLevel + other.levelCount);

        uint32_t ll = glm::max(baseArrayLayer, other.baseArrayLayer);
        uint32_t rl = glm::min(baseArrayLayer + layerCount, other.baseArrayLayer + other.layerCount);

        return {aspectMask, lm, glm::max(0u, rm < lm ? 0 : rm - lm), ll, glm::max(0u, rl < ll ? 0 : rl - ll)};
    }

    bool Overlaps(const ImageSubresourceRange& other)
    {
        ImageSubresourceRange temp = And(other);

        return temp.layerCount != 0 && temp.levelCount != 0;
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
class Image : public SafeReferenceable<Image>
{
public:
    Image(bool isGPUWrite) : isGPUWrite(isGPUWrite) {}
    virtual ~Image(){};
    virtual void SetName(std::string_view name) = 0;
    virtual const std::string& GetName() = 0;
    virtual const ImageDescription& GetDescription() = 0;
    virtual void SetData(std::span<uint8_t> binaryData, uint32_t mip = 0, uint32_t layer = 0) = 0;

    bool IsGPUWrite()
    {
        return isGPUWrite;
    }

    virtual ImageSubresourceRange GetSubresourceRange() = 0;
    virtual ImageView& GetDefaultImageView() = 0;
    virtual ImageLayout GetImageLayout() = 0;

protected:
    bool isGPUWrite;
};
} // namespace Gfx
