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
    std::optional<int> depth;
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
        }
    }

    void SetSubpass(int index, std::span<int> colors, std::optional<int> depth = std::nullopt)
    {
        if (index < subpasses.size())
        {
            subpasses[index].colors = std::vector<int>(colors.begin(), colors.end());
            subpasses[index].depth = depth;
        }
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
};
} // namespace Gfx::RG
