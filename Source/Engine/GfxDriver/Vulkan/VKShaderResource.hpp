#pragma once
#include "../DescriptorSetSlot.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKShaderInfo.hpp"
#include "VKSharedResource.hpp"
#include <unordered_map>
#include <vma/vk_mem_alloc.h>

namespace Engine::Gfx
{
class VKBuffer;
class VKImage;
class VKShaderProgram;
class VKDescriptorPool;
class VKDriver;

// actually a binding resource
class VKShaderResource : public ShaderResource
{
public:
    VKShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency);
    ~VKShaderResource() override;

    VkDescriptorSet GetDescriptorSet();
    RefPtr<ShaderProgram> GetShader() override;
    RefPtr<Buffer> GetBuffer(const std::string& object, BufferMemberInfoMap& memberInfo) override;
    bool HasPushConstnat(const std::string& obj) override;
    void SetTexture(const std::string& param, RefPtr<Image> image) override;
    void SetBuffer(Buffer& buffer, unsigned int binding, size_t offset = 0, size_t range = 0) override;
    DescriptorSetSlot GetDescriptorSetSlot() const
    {
        return slot;
    }

protected:
    using Binding = unsigned int;
    const std::unordered_map<std::string, VkPushConstantRange>* pushConstantRanges = nullptr;
    bool pushConstantIsUsed = false;

    RefPtr<VKShaderProgram> shaderProgram;
    RefPtr<VKSharedResource> sharedResource;
    RefPtr<VKDevice> device;

    VkDescriptorSet descriptorSet;
    VKDescriptorPool* descriptorPool = nullptr;
    DescriptorSetSlot slot;

    unsigned char* pushConstantBuffer = nullptr;
    std::unordered_map<Binding, Buffer*> buffers;
    std::unordered_map<std::string, RefPtr<VKImage>> textures;

    // TODO: deprecated
    std::unordered_map<std::string, UniPtr<VKBuffer>> uniformBuffers;
    // std::unordered_map<std::string, RefPtr<VKStorageBuffer>> storageBuffers;
    std::vector<std::function<void()>> pendingTextureUpdates;
};
} // namespace Engine::Gfx
