#pragma once

#include "Buffer.hpp"
#include "GfxDriver/ImageView.hpp"
#include "GfxEnums.hpp"
#include "Libs/Ptr.hpp"
#include "ResourceHandle.hpp"
#include "StorageBuffer.hpp"
#include <string>
#include <unordered_map>

namespace Gfx
{
class ShaderProgram;
class Image;

class ShaderResource
{
public:
    virtual void SetBuffer(ResourceHandle handle, Gfx::Buffer* buffer) = 0;
    virtual void SetImage(ResourceHandle handle, Gfx::Image* buffer) = 0;
    virtual void SetImage(ResourceHandle handle, Gfx::ImageView* imageView) = 0;
    virtual void SetImage(ResourceHandle handle, nullptr_t) = 0;

    void SetBuffer(std::string_view name, Gfx::Buffer* buffer)
    {
        SetBuffer(ResourceHandle(name), buffer);
    }
    void SetImage(std::string_view name, Gfx::Image* buffer)
    {
        SetImage(ResourceHandle(name), buffer);
    }
    void SetImage(std::string_view name, Gfx::ImageView* imageView)
    {
        SetImage(ResourceHandle(name), imageView);
    }
    void SetImage(std::string_view name, nullptr_t)
    {
        SetImage(ResourceHandle(name), nullptr);
    }

    virtual ~ShaderResource(){};

protected:
};
} // namespace Gfx
