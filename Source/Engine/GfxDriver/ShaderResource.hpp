#pragma once

#include "Buffer.hpp"
#include "GfxDriver/ImageView.hpp"
#include "GfxEnums.hpp"
#include "Libs/Ptr.hpp"
#include "StorageBuffer.hpp"
#include <string>
#include <unordered_map>

namespace Engine::Gfx
{
class ShaderProgram;
class Image;

class ShaderResource
{
public:
    ShaderResource(ShaderResourceFrequency frequency) : frequency(frequency) {}
    struct BufferMemberInfo
    {
        uint32_t offset;
        uint32_t size;
    };
    using BufferMemberInfoMap = std::unordered_map<std::string, BufferMemberInfo>;

    virtual ~ShaderResource(){};
    [[deprecated("Shader Resource will no longer manage life time of buffers")]]
    virtual RefPtr<Buffer> GetBuffer(const std::string& obj, BufferMemberInfoMap& memberInfo) = 0;

    // if range is zero, SetBuffer will use the size from buffer
    virtual void SetBuffer(Buffer& buffer, unsigned int binding, size_t offset = 0, size_t range = 0) = 0;
    virtual void SetImage(const std::string& name, RefPtr<Image> texture) = 0;
    virtual void SetImage(const std::string& name, ImageView* imageView) = 0;
    // virtual void SetStorage(std::string_view name, RefPtr<StorageBuffer>
    // storageBuffer) = 0;
    virtual bool HasPushConstnat(const std::string& obj) = 0;
    virtual RefPtr<ShaderProgram> GetShaderProgram() = 0;
    virtual ShaderResourceFrequency GetFrequency()
    {
        return frequency;
    }

protected:
    ShaderResourceFrequency frequency;
};
} // namespace Engine::Gfx
