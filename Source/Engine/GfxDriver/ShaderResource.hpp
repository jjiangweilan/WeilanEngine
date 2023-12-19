#pragma once

#include "Buffer.hpp"
#include "GfxDriver/ImageView.hpp"
#include "GfxEnums.hpp"
#include "Libs/Ptr.hpp"
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
    virtual void SetBuffer(const std::string& bindingName, Gfx::Buffer* buffer) = 0;
    virtual void SetImage(const std::string& bindingName, Gfx::Image* buffer) = 0;
    virtual void SetImage(const std::string& bindingName, Gfx::ImageView* imageView) = 0;
    virtual void SetImage(const std::string& bindingName, nullptr_t) = 0;

    virtual ~ShaderResource(){};

protected:
};
} // namespace Gfx
