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
    ResourceRef(Gfx::Image* image, void* owner) : data(image), owner(owner), type(ResourceType::Image) {}
    ResourceRef(Gfx::Buffer* buffer, void* owner) : data(buffer), owner(owner), type(ResourceType::Buffer) {}

    bool IsNull() { return data == nullptr; }
    bool IsType(ResourceType type) const { return this->type == type; }
    void* GetResource() const { return data; }

protected:
    void* owner;
    void* data;
    ResourceType type;
};

} // namespace Engine::RenderGraph
