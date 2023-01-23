#pragma once

#include "Buffer.hpp"
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
    virtual RefPtr<Buffer> GetBuffer(const std::string& obj, BufferMemberInfoMap& memberInfo) = 0;
    virtual void SetTexture(const std::string& name, RefPtr<Image> texture) = 0;
    // virtual void SetStorage(std::string_view name, RefPtr<StorageBuffer> storageBuffer) = 0;
    virtual bool HasPushConstnat(const std::string& obj) = 0;
    virtual RefPtr<ShaderProgram> GetShader() = 0;
    virtual ShaderResourceFrequency GetFrequency() { return frequency; }

protected:
    ShaderResourceFrequency frequency;
};
} // namespace Engine::Gfx
