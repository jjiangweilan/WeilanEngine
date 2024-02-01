#include "Image.hpp"
#include "ResourceHandle.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <optional>
#include <vector>

namespace Gfx::RG
{
struct AttachmentDescription
{
    AttachmentDescription(uint32_t width, uint32_t height, Gfx::ImageFormat format)
        : width(width), height(height), format(format)
    {
        Rehash();
    }

    void SetWidth(uint32_t width)
    {
        if (this->width != width)
        {
            this->width = width;
            Rehash();
        }
    }

    void SetHeight(uint32_t height)
    {
        if (this->height != height)
        {
            this->height = height;
            Rehash();
        }
    }

    void SetFormat(Gfx::ImageFormat format)
    {
        if (this->format != format)
        {
            this->format = format;
            Rehash();
        }
    }

    uint32_t GetWidth()
    {
        return width;
    }

    uint32_t GetHeight()
    {
        return height;
    }

    Gfx::ImageFormat GetFormat()
    {
        return format;
    }

    uint64_t GetHash()
    {
        return hash;
    }

private:
    uint32_t width;
    uint32_t height;
    Gfx::ImageFormat format;

    uint64_t hash;

    void Rehash()
    {
        hash = XXH3_64bits(this, sizeof(AttachmentDescription));
    }
};

struct AttachmentIdentifier
{
    enum class Type
    {
        None,
        Image,
        Handle
    };

    AttachmentIdentifier() : type(Type::None), image(nullptr) {}
    AttachmentIdentifier(Image& image) : type(Type::Image), image(&image) {}
    AttachmentIdentifier(ResourceHandle rthandle) : type(Type::Handle), rtHandle(rthandle) {}

    Type GetType()
    {
        return type;
    }

private:
    Type type;
    union
    {
        Image* image;
        ResourceHandle rtHandle;
    };
};

struct Subpass
{
    std::vector<int> colors;
    int depth;
};

class RenderPass
{
public:
    RenderPass(int subpassCount, int attachmentCount)
    {
        attachments.resize(attachmentCount);
        subpasses.resize(subpassCount);
    }

    void SetAttachment(int index, AttachmentIdentifier id)
    {
        if (index < attachments.size())
        {
            attachments[index] = id;
            Rehash();
        }
    }

    void SetSubpass(int index, std::span<int> colors, std::optional<int> depth = std::nullopt)
    {
        if (index < subpasses.size())
        {
            subpasses[index].colors = std::vector<int>(colors.begin(), colors.end());
            subpasses[index].depth = depth.value_or(-1);
            Rehash();
        }
    }

    uint64_t GetHash() const
    {
        return hash;
    }

    static RenderPass Default()
    {
        auto rp = RenderPass(1, 2);
        int colors[] = {0};
        int depth = 1;
        rp.SetSubpass(0, colors, depth);
        return rp;
    }

    static RenderPass SingleColor()
    {
        auto rp = RenderPass(1, 1);
        int colors[] = {0};
        rp.SetSubpass(0, colors);
        return rp;
    }

private:
    std::vector<AttachmentIdentifier> attachments;
    std::vector<Subpass> subpasses;

    uint64_t hash;

    void Rehash()
    {
        size_t sizeOfAttachments = attachments.size() * sizeof(AttachmentIdentifier);
        size_t sizeOfSubpasses = 0;
        for (auto& s : subpasses)
        {
            sizeOfSubpasses += s.colors.size() * sizeof(int);
            sizeOfSubpasses += sizeof(int); // depth
        }

        uint8_t* data = new uint8_t[sizeOfSubpasses + sizeOfAttachments];
        memcpy(data, attachments.data(), sizeOfAttachments);
        data += sizeOfAttachments;

        for (auto& s : subpasses)
        {
            size_t colorsSize = s.colors.size() * sizeof(int);
            memcpy(data, s.colors.data(), colorsSize);
            data += colorsSize;
            memcpy(data, &s.depth, sizeof(int));
        }

        hash = XXH3_64bits(data, sizeOfSubpasses + sizeOfAttachments);
        delete[] data;
    }
};
} // namespace Gfx::RG
