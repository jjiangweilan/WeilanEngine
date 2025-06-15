#pragma once

#include "Image.hpp"
#include "ImageView.hpp"
#include "Libs/Hash.hpp"
#include "Libs/UUID.hpp"
#include "ResourceHandle.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <optional>
#include "Libs/DynamicArray.hpp"

namespace Gfx::VK::RenderGraph
{
class Graph;
}

namespace Gfx::RG
{
struct ImageDescription
{
    ImageDescription() {}
    ImageDescription(uint32_t width, uint32_t height, Gfx::GfxFormat format, bool randomWrite = false)
        : data({width, height, format, randomWrite})
    {
        Rehash();
    }

    bool operator==(const ImageDescription& other) const { return data == other.data; }

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

    void SetFormat(Gfx::GfxFormat format)
    {
        if (data.format != format)
        {
            data.format = format;
            Rehash();
        }
    }

    uint32_t GetWidth() const { return data.width; }

    uint32_t GetHeight() const { return data.height; }

    Gfx::GfxFormat GetFormat() const { return data.format; }

    uint64_t GetHash() const { return hash; }

    bool GetRandomWrite() const { return data.randomWrite; }

private:
    struct InternalData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        Gfx::GfxFormat format = Gfx::GfxFormat::Invalid;
        bool randomWrite = false;
        bool operator==(const InternalData& other) const = default;
    } data;

    uint64_t hash;

    void Rehash() { hash = XXH3_64bits(&data, sizeof(InternalData)); }
};

struct ImageIdentifier
{
    static const ImageIdentifier& GetEmpty();

    enum class Type
    {
        None,
        Image,
        ImageView,
        Handle
    };

    ImageIdentifier() : type(Type::Handle), rtHandle(UUID()) {}
    ImageIdentifier(const char* name) : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(std::string_view name) : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(Image& image) : type(Type::Image), image(&image) {}
    ImageIdentifier(ImageView& imageView) : type(Type::ImageView), imageView(&imageView) {}

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
            else if (type == Type::ImageView)
            {
                return imageView == other.imageView;
            }
        }

        return true;
    }

    Type GetType() const { return type; }

    Image* GetAsImage() const { return image; }
    ImageView* GetAsImageView() const { return imageView; }
    UUID GetAsUUID() const { return rtHandle; }

    const std::string& GetName() const { return name; }

private:
    Type type = Type::None;
    std::string name = "";
    Image* image = nullptr;
    ImageView* imageView = nullptr;
    UUID rtHandle = UUID::GetEmptyUUID();

    void Copy(const ImageIdentifier& other)
    {
        name = other.name;
        image = other.image;
        rtHandle = other.rtHandle;
    }

    static ImageIdentifier CreateEmpty();
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
    DynamicArray<SubpassAttachment> colors;
    SubpassAttachment depth;
    bool operator==(const Subpass& other) const = default;
};

class RenderPass
{
public:
    RenderPass(
        const ImageIdentifier& color,
        Gfx::AttachmentLoadOperation colorLoadOp,
        Gfx::AttachmentStoreOperation colorStoreOp,
        const ImageIdentifier& depth,
        Gfx::AttachmentLoadOperation loadOp,
        Gfx::AttachmentStoreOperation storeOp
    )
        : name(std::to_string(GetDefaultNameId()++))
    {
        SetRenderTarget(color, colorLoadOp, colorStoreOp, depth, loadOp, storeOp);
    }

    RenderPass(
        const ImageIdentifier& attachment,
        Gfx::AttachmentLoadOperation attachmentLoadOp,
        Gfx::AttachmentStoreOperation attachmentStoreOp
    )
        : name(std::to_string(GetDefaultNameId()++))
    {
        SetRenderTarget(attachment, attachmentLoadOp, attachmentStoreOp);
    }

    RenderPass() : name(std::to_string(GetDefaultNameId()++)) { rehash = true; }
    RenderPass(int subpassCount, int attachmentCount) : name(std::to_string(GetDefaultNameId()++))
    {
        attachments.resize(attachmentCount, ImageIdentifier::GetEmpty());
        subpasses.resize(subpassCount);
        rehash = true;
    }

    RenderPass(std::string_view name, int subpassCount, int attachmentCount) : name(name)
    {
        attachments.resize(attachmentCount, ImageIdentifier::GetEmpty());
        subpasses.resize(subpassCount);
        rehash = true;
    }

    void SetAttachment(int index, const ImageIdentifier& id)
    {
        if (index < attachments.size())
        {
            if (attachments[index] != id)
            {
                attachments[index] = id;
                rehash = true;
            }
        }
    }

    void SetRenderTarget(
        const ImageIdentifier& attachment,
        Gfx::AttachmentLoadOperation attachmentLoadOp,
        Gfx::AttachmentStoreOperation attachmentStoreOp
    )
    {
        if (attachments.size() != 1)
        {
            attachments.resize(1, ImageIdentifier::GetEmpty());
        }
        if (subpasses.size() != 1)
        {
            subpasses.resize(1);
        }
        SetAttachment(0, attachment);

        SubpassAttachment attachments[] = {{0, attachmentLoadOp, attachmentStoreOp}};

        SetSubpass(0, attachments);
        rehash = true;
    }

    void SetRenderTarget(
        const ImageIdentifier& color,
        Gfx::AttachmentLoadOperation colorLoadOp,
        Gfx::AttachmentStoreOperation colorStoreOp,
        const ImageIdentifier& depth,
        Gfx::AttachmentLoadOperation loadOp,
        Gfx::AttachmentStoreOperation storeOp
    )
    {
        if (attachments.size() != 2)
        {
            attachments.resize(2, ImageIdentifier::GetEmpty());
        }
        if (subpasses.size() != 1)
        {
            subpasses.resize(1);
        }
        SetAttachment(0, color);
        SetAttachment(1, depth);

        SubpassAttachment colorAttachments[] = {{0, colorLoadOp, colorStoreOp, colorLoadOp, colorStoreOp}};
        SubpassAttachment depthAttachment = {0, colorLoadOp, colorStoreOp, colorLoadOp, colorStoreOp};

        SetSubpass(0, colorAttachments, depthAttachment);
        rehash = true;
    }

    bool IsValidForRendering() const;

    const DynamicArray<ImageIdentifier>& GetAttachments() { return attachments; }
    const DynamicArray<Subpass>& GetSubpasses() { return subpasses; }

    void SetSubpass(
        int index, std::span<SubpassAttachment> colors, std::optional<SubpassAttachment> depth = std::nullopt
    )
    {
        if (index < subpasses.size())
        {
            if (colors.size() != subpasses[index].colors.size() ||
                subpasses[index].depth != depth.value_or(SubpassAttachment()))
            {
                subpasses[index].colors = DynamicArray<SubpassAttachment>(colors.begin(), colors.end());
                subpasses[index].depth = depth.value_or(SubpassAttachment{-1});
                rehash = true;
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
                    subpasses[index].colors = DynamicArray<SubpassAttachment>(colors.begin(), colors.end());
                    subpasses[index].depth = depth.value_or(SubpassAttachment{-1});
                    rehash = true;
                }
            }
        }
    }

    uint64_t GetHash() const
    {
        if (rehash)
        {
            uint64_t hash = 0;
            for (auto& attachment : attachments)
            {
                Hash64(hash, attachment.GetType());
                switch (attachment.GetType())
                {
                    case ImageIdentifier::Type::Image:
                        {
                            uint64_t imageHash = std::hash<UUID>()(attachment.GetAsImage()->GetUUID());
                            Hash64(hash, imageHash);
                            break;
                        }
                    case ImageIdentifier::Type::ImageView:
                        {
                            uint64_t imageViewHash = std::hash<UUID>()(attachment.GetAsImageView()->GetUUID());
                            Hash64(hash, imageViewHash);
                            break;
                        }
                    case ImageIdentifier::Type::Handle:
                        {
                            Hash64(hash, std::hash<UUID>()(attachment.GetAsUUID()));
                            break;
                        }
                    default: break;
                }
            }

            for (auto& subpass : subpasses)
            {
                for (auto& color : subpass.colors)
                {
                    Hash64(hash, color.attachmentIndex);
                    Hash64(hash, color.loadOp);
                    Hash64(hash, color.storeOp);
                    Hash64(hash, color.stencilLoadOp);
                    Hash64(hash, color.stencilStoreOp);
                }

                Hash64(hash, subpass.depth.attachmentIndex);
                Hash64(hash, subpass.depth.loadOp);
                Hash64(hash, subpass.depth.storeOp);
                Hash64(hash, subpass.depth.stencilLoadOp);
                Hash64(hash, subpass.depth.stencilStoreOp);
            }

            this->hash = hash;
        }

        return hash;
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

    void SetName(std::string_view name) { this->name = name; }

    const std::string& GetName() { return name; }

    bool operator==(const RenderPass& other) const
    {
        return name == other.name && attachments == other.attachments && subpasses == other.subpasses;
    }

private:
    bool rehash = false;
    mutable uint64_t hash = 0;
    std::string name = "";
    DynamicArray<ImageIdentifier> attachments = {};
    DynamicArray<Subpass> subpasses = {};

    int& GetDefaultNameId()
    {
        static int id = 0;
        return id;
    };
};
} // namespace Gfx::RG
  //

template <>
struct std::hash<Gfx::RG::RenderPass>
{
    size_t operator()(const Gfx::RG::RenderPass& pass) const { return static_cast<size_t>(pass.GetHash()); }
};
