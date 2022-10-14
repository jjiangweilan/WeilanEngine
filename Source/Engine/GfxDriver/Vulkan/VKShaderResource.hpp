#pragma once
#include "GfxDriver/ShaderResource.hpp"
#include "VKDescriptorSetSlot.hpp"
#include "VKShaderInfo.hpp"
#include "VKSharedResource.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKDevice.hpp"
#include <vma/vk_mem_alloc.h>
#include <unordered_map>

namespace Engine::Gfx
{
    class VKBuffer;
    class VKImage;
    class VKShaderProgram;
    class VKDescriptorPool;
    class VKDriver;
    class VKShaderResource : public ShaderResource
    {
        public:
            VKShaderResource(
                    RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency);
            ~VKShaderResource() override;

            VkDescriptorSet GetDescriptorSet();
            RefPtr<ShaderProgram> GetShader() override;
            void SetUniform(std::string_view obj, std::string_view member, void* value) override;
            void SetTexture(const std::string& param, RefPtr<Image> image) override;
            DescriptorSetSlot GetDescriptorSetSlot() const { return slot; }
            void PrepareResource(VkCommandBuffer cmdBuf);
        protected:
            const std::unordered_map<std::string, VkPushConstantRange>* pushConstantRanges = nullptr;
            bool pushConstantIsUsed = false;

            RefPtr<VKShaderProgram> shaderProgram;
            RefPtr<VKSharedResource> sharedResource;
            RefPtr<VKDevice> device;

            VkDescriptorSet descriptorSet;
            VKDescriptorPool* descriptorPool = nullptr;
            DescriptorSetSlot slot;

            unsigned char* pushConstantBuffer = nullptr;
            std::unordered_map<std::string, RefPtr<VKImage>> textures;
            std::unordered_map<std::string, UniPtr<VKBuffer>> uniformBuffers;
    };
}
