#pragma once

#include "Buffer.hpp"
#include "Engine/Driver/GfxDriver/ImageView.hpp"
#include "GfxEnums.hpp"
#include "RayTracingContext.hpp"
#include "Engine/Core/Ptr.hpp"
#include "ResourceHandle.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
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
    virtual void SetName(std::string_view name) = 0;
    virtual const std::string& GetName() const = 0;

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
    void SetImage(ShaderBindingHandle handle, const Gfx::ImageIdentifier& imageId)
    {
        SetImage(handle, 0, imageId);
    }

    virtual void Remove(ShaderBindingHandle handle) = 0;

    virtual void SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer) = 0;
    virtual void SetImage(ShaderBindingHandle handle, int index, Gfx::Image* buffer) = 0;
    virtual void SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView) = 0;
    virtual void SetImage(ShaderBindingHandle handle, int index, const Gfx::ImageIdentifier& imageId) = 0;
    virtual void SetAccelerationStructure(ShaderBindingHandle handle, int index, RayTracingContext* context, RayTracingSceneHandle scene) = 0;
    virtual void RebuildAll() = 0;
    virtual void Clear() = 0;

    void Remove(std::string_view name)
    {
        Remove(ShaderBindingHandle(name));
    }
    void SetBuffer(std::string_view name, Gfx::Buffer* buffer)
    {
        SetBuffer(ShaderBindingHandle(name), buffer);
    }
    void SetImage(std::string_view name, Gfx::Image* image)
    {
        SetImage(ShaderBindingHandle(name), image);
    }
    void SetImage(std::string_view name, Gfx::ImageView* imageView)
    {
        SetImage(ShaderBindingHandle(name), imageView);
    }

    void SetAccelerationStructure(std::string_view name, int index, RayTracingContext* context, RayTracingSceneHandle scene)
    {
        SetAccelerationStructure(ShaderBindingHandle(name), index, context, scene);
    }


    virtual ~ShaderResource(){};

protected:
};
} // namespace Gfx
