#pragma once

#include "Image.hpp"
#include "Libs/UUID.hpp"
#include "ResourceHandle.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <optional>
#include <vector>

namespace Gfx::RG
{
struct ImageDescription
{
    ImageDescription() {}
    ImageDescription(uint32_t width, uint32_t height, Gfx::ImageFormat format, bool randomWrite = false)
        : data({width, height, format, randomWrite})
    {
        Rehash();
    }

    bool operator==(const ImageDescription& other) const
    {
        return data == other.data;
    }

    void SetRandomWrite(bool enable)
    {
        if (data.randomWrite != enable)
        {
            data.randomWrite = enable;
            Rehash();
        }
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

    uint32_t GetWidth() const
    {
        return data.width;
    }

    uint32_t GetHeight() const
    {
        return data.height;
    }

    Gfx::ImageFormat GetFormat() const
    {
        return data.format;
    }

    uint64_t GetHash() const
    {
        return hash;
    }

    bool GetRandomWrite() const
    {
        return data.randomWrite;
    }

private:
    struct InternalData
    {
        uint32_t width;
        uint32_t height;
        Gfx::ImageFormat format;
        bool randomWrite = false;
        bool operator==(const InternalData& other) const = default;
    } data;

    uint64_t hash;

    void Rehash()
    {
        hash = XXH3_64bits(&data, sizeof(InternalData));
    }
};

struct ImageIdentifier
{
    enum class Type
    {
        None,
        Image,
        Handle
    };

    ImageIdentifier() : type(Type::Handle), rtHandle(UUID()) {}
    ImageIdentifier(std::string_view name) : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(Image& image) : type(Type::Image), image(&image) {}

    bool operator==(const ImageIdentifier& other) const
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

            return false;
        }
    }

    Type GetType() const
    {
        return type;
    }

    Image* GetAsImage() const
    {
        return image;
    }

    UUID GetAsUUID() const
    {
        return rtHandle;
    }

    const std::string& GetName() const
    {
        return name;
    }

private:
    Type type = Type::None;
    std::string name = "";
    Image* image = nullptr;
    UUID rtHandle = UUID::GetEmptyUUID();

    void Copy(const ImageIdentifier& other)
    {
        name = other.name;
        image = other.image;
        rtHandle = other.rtHandle;
    }
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
    RenderPass() : uuid(), name(std::to_string(GetDefaultNameId()++)) {}
    RenderPass(int subpassCount, int attachmentCount) : uuid(), name(std::to_string(GetDefaultNameId()++))
    {
        attachments.resize(attachmentCount);
        subpasses.resize(subpassCount);
    }

    void SetAttachment(int index, const ImageIdentifier& id)
    {
        if (index < attachments.size())
        {
            if (attachments[index] != id)
            {
                attachments[index] = id;
                uuid = UUID();
            }
        }
    }

    const std::vector<ImageIdentifier>& GetAttachments()
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
                uuid = UUID();
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
                    uuid = UUID();
                }
            }
        }
    }

    const UUID& GetUUID() const
    {
        return uuid;
    }

    static RenderPass Default(
        std::string_view name = "default render pass",
        Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear,
        Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation depthLoadOp = Gfx::AttachmentLoadOperation::Clear,
        Gfx::AttachmentStoreOperation depthStoreOp = Gfx::AttachmentStoreOperation::Store
    )
    {
        auto rp = RenderPass(1, 2);
        SubpassAttachment colors[] = {{0, loadOp, storeOp}};
        SubpassAttachment depth = {1, depthLoadOp, depthStoreOp};
        rp.SetSubpass(0, colors, depth);
        rp.SetName(name);
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
    std::vector<ImageIdentifier> attachments;
    std::vector<Subpass> subpasses;

    UUID uuid;

    int& GetDefaultNameId()
    {
        static int id = 0;
        return id;
    };
};
} // namespace Gfx::RG
