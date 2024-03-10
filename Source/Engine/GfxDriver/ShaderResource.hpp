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
    void SetBuffer(ShaderBindingHandle handle, Gfx::Buffer* buffer)
    {
        SetBuffer(handle, 0, buffer);
    }
    void SetImage(ShaderBindingHandle handle, Gfx::Image* buffer)
    {
        SetImage(handle, 0, buffer);
    }
    void SetImage(ShaderBindingHandle handle, Gfx::ImageView* imageView)
    {
        SetImage(handle, 0, imageView);
    }

    virtual void Remove(ShaderBindingHandle handle) = 0;

    virtual void SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer) = 0;
    virtual void SetImage(ShaderBindingHandle handle, int index, Gfx::Image* buffer) = 0;
    virtual void SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView) = 0;

    void Remove(std::string_view name)
    {
        Remove(ShaderBindingHandle(name));
    }
    void SetBuffer(std::string_view name, Gfx::Buffer* buffer)
    {
        SetBuffer(ShaderBindingHandle(name), buffer);
    }
    void SetImage(std::string_view name, Gfx::Image* buffer)
    {
        SetImage(ShaderBindingHandle(name), buffer);
    }
    void SetImage(std::string_view name, Gfx::ImageView* imageView)
    {
        SetImage(ShaderBindingHandle(name), imageView);
    }

    virtual ~ShaderResource(){};

protected:
};
} // namespace Gfx
