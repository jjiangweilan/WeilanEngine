#include "Image.hpp"
#include "ResourceHandle.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <optional>
#include <vector>

namespace Gfx::RG
{
struct AttachmentDescription
{
    AttachmentDescription(uint32_t width, uint32_t height, Gfx::ImageFormat format) : data({width, height, format})
    {
        Rehash();
    }

    void SetWidth(uint32_t width)
    {
        if (data.width != width)
        {
            data.width = width;
            Rehash();
        }
    }

    void SetHeight(uint32_t height)
    {
        if (data.height != height)
        {
            data.height = height;
            Rehash();
        }
    }

    void SetFormat(Gfx::ImageFormat format)
    {
        if (data.format != format)
        {
            data.format = format;
            Rehash();
        }
    }

    uint32_t GetWidth()
    {
        return data.width;
    }

    uint32_t GetHeight()
    {
        return data.height;
    }

    Gfx::ImageFormat GetFormat()
    {
        return data.format;
    }

    uint64_t GetHash()
    {
        return hash;
    }

private:
    struct InternalData
    {
        uint32_t width;
        uint32_t height;
        Gfx::ImageFormat format;
    } data;

    uint64_t hash;

    void Rehash()
    {
        hash = XXH3_64bits(&data, sizeof(InternalData));
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
    AttachmentIdentifier(uint64_t rgHash) : type(Type::Handle), rtHandle(rgHash) {}

    bool operator==(const AttachmentIdentifier& other) const
    {
        if (type != other.type)
        {
            return false;
        }
        else
        {
            if (type == Type::Image)
            {
                return image == other.image;
            }
            else if (type == Type::Handle)
            {
                return rtHandle == other.rtHandle;
            }

            return true;
        }
    }

    Type GetType()
    {
        return type;
    }

    Image* GetAsImage()
    {
        return image;
    }

    uint64_t GetAsHash()
    {
        return rtHandle;
    }

private:
    Type type;
    union
    {
        Image* image;
        uint64_t rtHandle;
    };
};

struct SubpassAttachment
{
    bool operator==(const SubpassAttachment& other) const = default;
    int attachmentIndex = -1;
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::Store;
};

struct Subpass
{
    std::vector<SubpassAttachment> colors;
    SubpassAttachment depth;
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
            if (attachments[index] != id)
            {
                attachments[index] = id;
                Rehash();
            }
        }
    }

    const std::vector<AttachmentIdentifier>& GetAttachments()
    {
        return attachments;
    }
    const std::vector<Subpass>& GetSubpasses()
    {
        return subpasses;
    }

    void SetSubpass(
        int index, std::span<SubpassAttachment> colors, std::optional<SubpassAttachment> depth = std::nullopt
    )
    {
        if (index < subpasses.size())
        {
            if (colors.size() != subpasses[index].colors.size() ||
                subpasses[index].depth != depth.value_or(SubpassAttachment()))
            {
                subpasses[index].colors = std::vector<SubpassAttachment>(colors.begin(), colors.end());
                subpasses[index].depth = depth.value_or(SubpassAttachment{-1});
                Rehash();
            }
            else
            {
                bool diff = false;
                for (int i = 0; i < colors.size(); ++i)
                {
                    diff = diff || colors[i] != subpasses[index].colors[i];
                }

                if (diff)
                {
                    subpasses[index].colors = std::vector<SubpassAttachment>(colors.begin(), colors.end());
                    subpasses[index].depth = depth.value_or(SubpassAttachment{-1});
                    Rehash();
                }
            }
        }
    }

    uint64_t GetHash() const
    {
        return hash;
    }

    static RenderPass Default()
    {
        auto rp = RenderPass(1, 2);
        SubpassAttachment colors[] = {{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}};
        SubpassAttachment depth = {1, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
        rp.SetSubpass(0, colors, depth);
        return rp;
    }

    static RenderPass SingleColor(
        std::string_view name = "single color render pass",
        Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear,
        Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store
    )
    {
        auto rp = RenderPass(1, 1);
        SubpassAttachment colors[] = {{0, loadOp, storeOp}};
        rp.SetSubpass(0, colors);
        rp.SetName(name);
        return rp;
    }

    void SetName(std::string_view name)
    {
        this->name = name;
    }

    const std::string& GetName()
    {
        return name;
    }

private:
    std::string name;
    std::vector<AttachmentIdentifier> attachments;
    std::vector<Subpass> subpasses;

    uint64_t hash;

    void Rehash()
    {
        size_t sizeOfAttachments = attachments.size() * sizeof(AttachmentIdentifier);
        size_t sizeOfSubpasses = 0;
        for (auto& s : subpasses)
        {
            sizeOfSubpasses += s.colors.size() * sizeof(SubpassAttachment);
            sizeOfSubpasses += sizeof(SubpassAttachment); // depth
        }

        uint8_t* data = new uint8_t[sizeOfSubpasses + sizeOfAttachments];
        uint8_t* ori = data;
        memcpy(data, attachments.data(), sizeOfAttachments);
        data += sizeOfAttachments;

        for (auto& s : subpasses)
        {
            size_t colorsSize = s.colors.size() * sizeof(SubpassAttachment);
            memcpy(data, s.colors.data(), colorsSize);
            data += colorsSize;
            memcpy(data, &s.depth, sizeof(SubpassAttachment));
        }

        hash = XXH3_64bits(data, sizeOfSubpasses + sizeOfAttachments);
        delete[] ori;
    }
};
} // namespace Gfx::RG