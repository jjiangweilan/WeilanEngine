#pragma once
#include "GfxDriver/ShaderResource.hpp"
#include "../VKDescriptorSetSlot.hpp"
#include "../VKShaderInfo.hpp"
#include <vma/vk_mem_alloc.h>
#include <unordered_map>

namespace Engine::Gfx
{
    struct VKContext;
    class VKImage;
    class VKShaderProgram;
    class VKDescriptorPool;
}
namespace Engine::Gfx::Exp
{
    class VKBuffer;
    class VKShaderResource : public ShaderResource
    {
        public:
            VKShaderResource(RefPtr<VKContext> context, RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency);
            ~VKShaderResource() override;

            VkDescriptorSet GetDescriptorSet();
            RefPtr<ShaderProgram> GetShader() override;
            void SetUniform(std::string_view obj, std::string_view member, void* value) override;
            void SetTexture(const std::string& param, RefPtr<Image> image) override;
            void UpdatePushConstant(VkCommandBuffer cmd);
            DescriptorSetSlot GetDescriptorSetSlot() const { return slot; }
            void PrepareResource(VkCommandBuffer cmdBuf);
        protected:
            const std::unordered_map<std::string, VkPushConstantRange>* pushConstantRanges = nullptr;
            bool pushConstantIsUsed = false;

            RefPtr<VKContext> context;
            RefPtr<VKShaderProgram> shaderProgram;

            VkDescriptorSet descriptorSet;
            VKDescriptorPool* descriptorPool = nullptr;
            DescriptorSetSlot slot;

            unsigned char* pushConstantBuffer = nullptr;
            std::unordered_map<std::string, RefPtr<VKImage>> textures;
            std::unordered_map<std::string, UniPtr<VKBuffer>> uniformBuffers;
    };
}
