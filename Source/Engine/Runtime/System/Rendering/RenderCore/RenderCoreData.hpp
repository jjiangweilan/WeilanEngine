#pragma
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include <vk_mem_alloc.h> // for virtual memory allocator

struct RenderImageDescriptor
{
    RenderImageDescriptor() {}
    RenderImageDescriptor(uint32_t width, uint32_t height, Gfx::GfxFormat format, bool randomWrite = false)
        : data({width, height, format, randomWrite})
    {
        Rehash();
    }

    bool operator==(const RenderImageDescriptor& other) const { return data == other.data; }

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

    ImageIdentifier(const ImageIdentifier& other) = default;
    ImageIdentifier()
        : type(Type::Handle), rtHandle(UUID()) {}
    ImageIdentifier(const char* name)
        : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(const std::string& name)
        : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(std::string_view name)
        : type(Type::Handle), name(name), rtHandle(UUID()) {}
    ImageIdentifier(Gfx::Image& image)
        : type(Type::Image), image(&image) {}
    ImageIdentifier(Gfx::ImageView& imageView)
        : type(Type::ImageView), imageView(&imageView) {}

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

    Gfx::Image* GetAsImage() const { return image; }
    Gfx::ImageView* GetAsImageView() const { return imageView; }
    UUID GetAsUUID() const { return rtHandle; }

    const std::string& GetName() const { return name; }

private:
    Type type = Type::None;
    std::string name = "";
    Gfx::Image* image = nullptr;
    Gfx::ImageView* imageView = nullptr;
    UUID rtHandle = UUID::GetEmptyUUID();

    void Copy(const ImageIdentifier& other)
    {
        name = other.name;
        image = other.image;
        rtHandle = other.rtHandle;
    }

    static ImageIdentifier CreateEmpty();
};

struct MeshHandle
{
    VmaVirtualAllocation vertexHandle = 0;
    uint64_t vertexOffset = 0;

    VmaVirtualAllocation indexHandle = 0;
    uint64_t indexOffset = 0;
};

struct RenderAttachment
{
    ImageIdentifier image;
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::Store;
};
