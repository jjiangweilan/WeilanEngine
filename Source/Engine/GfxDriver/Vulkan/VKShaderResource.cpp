#include "VKShaderResource.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKDriver.hpp"
#include "VKShaderProgram.hpp"
#include "VKSharedResource.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace Gfx
{
DescriptorSetSlot MapDescriptorSetSlot(ShaderResourceFrequency frequency)
{
    switch (frequency)
    {
        case ShaderResourceFrequency::Global: return Global_Descriptor_Set;
        case ShaderResourceFrequency::Pass: return Shader_Descriptor_Set;
        case ShaderResourceFrequency::Material: return Material_Descriptor_Set;
        case ShaderResourceFrequency::Object: return Object_Descriptor_Set;
        default: return Material_Descriptor_Set;
    }
}

VKShaderResource::VKShaderResource() : sharedResource(VKContext::Instance()->sharedResource), sets(0) {}

VKShaderResource::~VKShaderResource()
{
    for (auto& d : sets)
    {
        descriptorPool->Deallocate(d.second.set);
    }
}

void VKShaderResource::RebuildAll()
{
    for (auto& d : sets)
    {
        d.second.rebuild = true;
    }
}

void VKShaderResource::SetBuffer(const std::string& name, Gfx::Buffer* buffer)
{
    bindings[name] = {buffer, ResourceType::Buffer};
    RebuildAll();
}

void VKShaderResource::SetImage(const std::string& name, Gfx::Image* image)
{
    bindings[name] = {&image->GetDefaultImageView(), ResourceType::ImageView};
    RebuildAll();
}

void VKShaderResource::SetImage(const std::string& name, Gfx::ImageView* imageView)
{
    bindings[name] = {imageView, ResourceType::ImageView};
    RebuildAll();
}

void VKShaderResource::SetImage(const std::string& name, nullptr_t)
{
    bindings.erase(name);
    RebuildAll();
}

VkDescriptorSet VKShaderResource::GetDescriptorSet(uint32_t set, VKShaderProgram* shaderProgram)
{
    if (shaderProgram == nullptr || !shaderProgram->HasSet(set))
        return VK_NULL_HANDLE;
    auto iter = sets.find(shaderProgram);

    VkDescriptorSet finalReturn = VK_NULL_HANDLE;
    bool rebuild = false;
    std::vector<VKWritableGPUResource>* writableGPUResources;
    if (iter == sets.end())
    {
        layout = shaderProgram->GetVKPipelineLayout();
        descriptorPool = &shaderProgram->GetDescriptorPool(set);
        VkDescriptorSet descriptorSet = descriptorPool->Allocate();
        finalReturn = descriptorSet;
        rebuild = true;
        sets[shaderProgram] = {set, descriptorSet, false};
        writableGPUResources = &sets[shaderProgram].writableGPUResources;
    }
    else
    {
        if (iter->second.creationSetIndex != set)
        {
            SPDLOG_ERROR("shader resource is binded to a different set, this is not allowed");
            return VK_NULL_HANDLE;
        }
        rebuild = iter->second.rebuild;
        if (rebuild)
        {
            descriptorPool = &shaderProgram->GetDescriptorPool(set);
            finalReturn = descriptorPool->Allocate();
            iter->second.set = finalReturn;
            descriptorPool->Deallocate(iter->second.set);
            iter->second.rebuild = false;
            writableGPUResources = &iter->second.writableGPUResources;
        }
    }

    if (rebuild)
    {
        writableGPUResources->clear();
        auto& shaderInfo = shaderProgram->GetShaderInfo();
        VKDebugUtils::SetDebugName(
            VK_OBJECT_TYPE_DESCRIPTOR_SET,
            (uint64_t)finalReturn,
            (shaderProgram->GetName() + std::to_string(set)).c_str()
        );

        // create resources and write it to descriptor set
        VkWriteDescriptorSet writes[32];
        VkDescriptorBufferInfo bufferInfos[32];
        VkDescriptorImageInfo imageInfos[32];
        uint32_t bufferWriteIndex = 0;
        uint32_t imageWriteIndex = 0;
        uint32_t writeCount = 0;
        auto iter = shaderInfo.descriptorSetBinidngMap.find(set);
        if (iter != shaderInfo.descriptorSetBinidngMap.end())
        {
            assert(iter->second.bindings.size() < 64);

            for (auto& b : iter->second.bindings)
            {
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].pNext = VK_NULL_HANDLE;
                writes[writeCount].dstSet = finalReturn;
                writes[writeCount].descriptorType = ShaderInfo::Utils::MapBindingType(b.second.type);
                writes[writeCount].dstBinding = b.second.bindingNum;
                writes[writeCount].dstArrayElement = 0;
                writes[writeCount].descriptorCount = b.second.count;
                writes[writeCount].pImageInfo = VK_NULL_HANDLE;
                writes[writeCount].pBufferInfo = VK_NULL_HANDLE;
                writes[writeCount].pTexelBufferView = VK_NULL_HANDLE;

                ResourceRef resRef = bindings[b.second.name];

                switch (b.second.type)
                {
                    case ShaderInfo::BindingType::UBO:
                    case ShaderInfo::BindingType::SSBO:
                        {
                            for (int i = 0; i < writes[writeCount].descriptorCount; ++i)
                            {
                                VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                                VKBuffer* buffer = nullptr;
                                if (resRef.type != ResourceType::Buffer || resRef.res == nullptr)
                                {
                                    std::string bufferName =
                                        fmt::format("Default Buffer for {}", shaderProgram->GetName());
                                    Buffer::CreateInfo createInfo{
                                        .usages =
                                            (b.second.type == ShaderInfo::BindingType::UBO ? BufferUsage::Uniform
                                                                                           : BufferUsage::Storage) |
                                            BufferUsage::Transfer_Dst,
                                        .size = b.second.binding.ubo.data.size,
                                        .visibleInCPU = false,
                                        .debugName = bufferName.c_str()};
                                    auto defaultBuffer = std::make_unique<VKBuffer>(createInfo);
                                    buffer = defaultBuffer.get();
                                    buffers.insert(std::make_pair(b.first, std::move(defaultBuffer)));
                                }
                                else
                                {
                                    buffer = (VKBuffer*)resRef.res;
                                }

                                if (buffer->IsGPUWrite())
                                {
                                    VkPipelineStageFlags pipelineStages = 0;
                                    if (b.second.stages & Gfx::ShaderInfo::ShaderStage::Vert)
                                        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                                    if (b.second.stages & Gfx::ShaderInfo::ShaderStage::Frag)
                                        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Buffer,
                                        .data = buffer,
                                        .stages = pipelineStages,
                                        .access = static_cast<VkAccessFlags>(
                                            VK_ACCESS_SHADER_READ_BIT |
                                            (b.second.type == ShaderInfo::BindingType::SSBO ? VK_ACCESS_SHADER_WRITE_BIT
                                                                                            : 0)
                                        ),
                                    };

                                    writableGPUResources->push_back(gpuResource);
                                }
                                bufferInfo.buffer = buffer->GetHandle();
                                bufferInfo.offset = i * b.second.binding.ubo.data.size;
                                bufferInfo.range = b.second.binding.ubo.data.size;
                                writes[writeCount].pBufferInfo = &bufferInfo;
                            }
                            break;
                        }
                    case ShaderInfo::BindingType::Texture:
                    case ShaderInfo::BindingType::SeparateImage:
                        {
                            VKImageView* imageView = (VKImageView*)resRef.res;
                            if (b.second.binding.texture.type == ShaderInfo::Texture::Type::Tex2D)
                            {
                                VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = b.second.type == ShaderInfo::BindingType::Texture
                                                        ? sharedResource->GetDefaultSampler()
                                                        : VK_NULL_HANDLE;
                                if (resRef.res != nullptr && !imageView->GetImage().GetDescription().isCubemap &&
                                    resRef.type == ResourceType::ImageView)
                                {
                                    imageInfo.imageView = imageView->GetHandle();
                                }
                                else
                                {
                                    imageInfo.imageView =
                                        sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                                }
                                writes[writeCount].pImageInfo = &imageInfo;
                            }
                            else if (b.second.binding.texture.type == ShaderInfo::Texture::Type::TexCube)
                            {
                                VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = sharedResource->GetDefaultSampler();

                                if (resRef.res != nullptr && imageView->GetImage().GetDescription().isCubemap &&
                                    resRef.type == ResourceType::ImageView)
                                {
                                    imageInfo.imageView = imageView->GetHandle();
                                }
                                else
                                {
                                    imageInfo.imageView =
                                        sharedResource->GetDefaultTextureCube()->GetDefaultVkImageView();
                                }

                                writes[writeCount].pImageInfo = &imageInfo;
                            }

                            if (imageView && imageView->GetImage().IsGPUWrite())
                            {
                                VkPipelineStageFlags pipelineStages = 0;
                                if (b.second.stages & Gfx::ShaderInfo::ShaderStage::Vert)
                                    pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                                if (b.second.stages & Gfx::ShaderInfo::ShaderStage::Frag)
                                    pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                                VKWritableGPUResource gpuResource{
                                    .type = VKWritableGPUResource::Type::Image,
                                    .data = &imageView->GetImage(),
                                    .stages = pipelineStages,
                                    .access = VK_ACCESS_SHADER_READ_BIT,
                                    .imageView = imageView,
                                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

                                writableGPUResources->push_back(gpuResource);
                            }
                            break;
                        }
                    case ShaderInfo::BindingType::SeparateSampler:
                        {
                            auto createInfo = SamplerCachePool::GenerateSamplerCreateInfoFromString(
                                b.first,
                                b.second.binding.separateSampler.enableCompare
                            );
                            VkSampler sampler = SamplerCachePool::RequestSampler(createInfo);
                            VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.sampler = sampler;
                            imageInfo.imageView = VK_NULL_HANDLE;
                            writes[writeCount].pImageInfo = &imageInfo;
                            break;
                        }
                    default: assert(0 && "Not implemented"); break;
                }

                writeCount += 1;
            }
        }

        vkUpdateDescriptorSets(GetDevice(), writeCount, writes, 0, VK_NULL_HANDLE);
        return finalReturn;
    }

    return iter->second.set;
}

const std::vector<VKWritableGPUResource>& VKShaderResource::GetWritableResources(
    uint32_t set, VKShaderProgram* shaderProgram
)
{
    auto iter = sets.find(shaderProgram);
    if (iter == sets.end())
    {
        GetDescriptorSet(set, shaderProgram);
        iter = sets.find(shaderProgram);
    }
    if (iter != sets.end() && iter->second.creationSetIndex == set)
    {
        return iter->second.writableGPUResources;
    }

    static std::vector<VKWritableGPUResource> empty;
    return empty;
}
} // namespace Gfx
