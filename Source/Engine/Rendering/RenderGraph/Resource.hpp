#pragma once
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/Image.hpp"
#include <vector>

namespace Engine::RenderGraph
{
enum class ResourceType
{
    Image,
    Buffer
};

class ResourceOwner;
class ResourceRef
{
public:
    ResourceRef() : owner(nullptr), data(nullptr), type(ResourceType::Buffer) {}
    ResourceRef(ResourceType type, void* owner) : data(nullptr), owner(owner), type(type) {}

    void SetResource(Gfx::Image* image)
    {
        this->data = image;
    }

    void SetResource(Gfx::Buffer* buffer)
    {
        this->data = buffer;
    }

    bool IsNull()
    {
        return data == nullptr;
    }
    bool IsType(ResourceType type) const
    {
        return this->type == type;
    }
    void* GetResource() const
    {
        return data;
    }
    void* GetOwner() const
    {
        return owner;
    }

protected:
    void* owner;
    void* data;
    ResourceType type;
};

} // namespace Engine::RenderGraph
