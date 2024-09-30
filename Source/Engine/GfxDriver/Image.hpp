#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include "Core/SafeReferenceable.hpp"
#include "ImageDescription.hpp"
#include "Libs/UUID.hpp"
#include "ThirdParty/xxHash/xxhash.h"
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

// treat this as a 2D data
// mip level as x, layer as y
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

        std::vector<ImageSubresourceRange> rtn{};
        // bottom area
        if (other.baseArrayLayer > baseArrayLayer && other.baseMipLevel >= baseMipLevel &&
            other.baseMipLevel < baseMipLevel + levelCount)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                baseMipLevel,
                levelCount,
                baseArrayLayer,
                other.baseArrayLayer - baseArrayLayer
            });
        }

        // top area
        if (other.baseArrayLayer + other.layerCount < baseArrayLayer + layerCount &&
            other.baseMipLevel >= baseMipLevel && other.baseMipLevel < baseMipLevel + levelCount)
        {
            rtn.push_back(ImageSubresourceRange{
                aspectMask,
                baseMipLevel,
                levelCount,
                other.baseArrayLayer + other.layerCount,
                baseArrayLayer + layerCount - (other.baseArrayLayer + other.layerCount)
            });
        }

        uint32_t top = glm::min(other.baseArrayLayer + other.layerCount, baseArrayLayer + layerCount);
        uint32_t bottom = glm::max(other.baseArrayLayer, baseArrayLayer);
        uint32_t height = top - bottom;
        // left area
        {
            uint32_t width = other.baseMipLevel - baseMipLevel;
            if (height > 0 && height <= layerCount && width > 0)
            {
                rtn.push_back(ImageSubresourceRange{aspectMask, baseMipLevel, width, bottom, height});
            }
        }

        // right area
        {
            uint32_t width = other.baseMipLevel + other.levelCount - (baseMipLevel + levelCount);
            if (height > 0 && height <= layerCount && width < 0)
            {
                rtn.push_back(
                    ImageSubresourceRange{aspectMask, other.baseMipLevel + other.levelCount, -width, bottom, height}
                );
            }
        }

        // totally ouside
        if (rtn.empty() && *this != other)
        {
            rtn.push_back(*this);
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

struct ImageViewOption
{
    int baseMipLevel;
    int levelCount;
    int baseArrayLayer;
    int layerCount;
};

class ImageView;
class Image : public SafeReferenceable<Image>
{
public:
    Image(bool isGPUWrite) : uuid(), isGPUWrite(isGPUWrite) {}
    virtual ~Image() {};
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
    virtual ImageView& GetImageView(const ImageViewOption& option) = 0;
    virtual ImageLayout GetImageLayout() = 0;

    virtual const UUID& GetUUID()
    {
        return uuid;
    }

protected:
    UUID uuid;
    bool isGPUWrite;
};
} // namespace Gfx
  //

template <>
struct std::hash<Gfx::ImageSubresourceRange>
{
    std::size_t operator()(Gfx::ImageSubresourceRange const& s) const noexcept
    {
        return XXH3_64bits(&s, sizeof(Gfx::ImageSubresourceRange));
    }
};
