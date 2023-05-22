#pragma once
#include "GfxDriver/Image.hpp"
#include <memory>
#include <unordered_map>
namespace Engine::RenderGraph
{
enum class ResourceType
{
    Null,
    Image, // Gfx::Image
};

struct ResourceRef
{
    ResourceRef() : type(ResourceType::Null), resource(nullptr) {}
    ResourceRef(nullptr_t) : resource(nullptr), type(ResourceType::Null) {}
    ResourceRef& operator=(nullptr_t)
    {
        type = ResourceType::Null;
        resource = nullptr;
        return *this;
    };
    bool operator!=(ResourceRef other) { return resource != other.resource; }
    ResourceType type = ResourceType::Null;
    void* resource = nullptr;
};
class GraphContext
{
public:
    ResourceRef DeclareImage(const Gfx::ImageDescription& desc) {}

private:
};
} // namespace Engine::RenderGraph
