#include "Image.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <optional>
#include <vector>

namespace Gfx::RG
{
class ResourceHandle
{
public:
    ResourceHandle() : hash(0) {}
    ResourceHandle(std::string_view name) : hash(XXH64((void*)name.data(), name.size(), 0)) {}

    uint64_t operator()() const
    {
        return hash;
    }

private:
    uint64_t hash;
};
using AttachmentHandle = size_t;
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
    AttachmentIdentifier(AttachmentHandle rthandle) : type(Type::Handle), rtHandle(rthandle) {}

    Type GetType()
    {
        return type;
    }

private:
    Type type;
    union
    {
        Image* image;
        size_t rtHandle;
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
