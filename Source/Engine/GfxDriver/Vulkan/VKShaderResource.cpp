#include "VKShaderResource.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKDriver.hpp"
#include "VKShaderProgram.hpp"
#include "VKSharedResource.hpp"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace Gfx
{
static VkPipelineStageFlags ShaderStageToPipelineStage(ShaderInfo::ShaderStageFlags stages)
{
    using namespace ShaderInfo;
    VkPipelineStageFlags pipelineStages = 0;
    if (stages & ShaderStage::Vert)
        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (stages & ShaderStage::Frag)
        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (stages & ShaderStage::Comp)
        pipelineStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    return pipelineStages;
}
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

void VKShaderResource::SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != buffer)
    {
        if (buffer == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = {buffer->GetSRef(), ShaderBindingType::Buffer};
        RebuildAll();
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, Gfx::Image* image)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != &image->GetDefaultImageView())
    {
        if (image == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = {image->GetDefaultImageView().GetSRef(), ShaderBindingType::ImageView};
        RebuildAll();
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != imageView)
    {
        if (imageView == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = {imageView->GetSRef(), ShaderBindingType::ImageView};
        RebuildAll();
    }
}

void VKShaderResource::Remove(ShaderBindingHandle handle)
{
    if (bindings.contains(handle))
    {
        bindings.erase(handle);
        RebuildAll();
    }
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
        SPDLOG_INFO("VKShaderResource: rebuild descriptor set");
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
        auto iter = shaderInfo.descriptorSetBindingMap.find(set);
        if (iter != shaderInfo.descriptorSetBindingMap.end())
        {
            assert(iter->second.size() < 64);

            for (auto b : iter->second)
            {
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].pNext = VK_NULL_HANDLE;
                writes[writeCount].dstSet = finalReturn;
                writes[writeCount].descriptorType = ShaderInfo::Utils::MapBindingType(b->type);
                writes[writeCount].dstBinding = b->bindingNum;
                writes[writeCount].dstArrayElement = 0;
                writes[writeCount].descriptorCount = b->count;
                writes[writeCount].pImageInfo = VK_NULL_HANDLE;
                writes[writeCount].pBufferInfo = VK_NULL_HANDLE;
                writes[writeCount].pTexelBufferView = VK_NULL_HANDLE;

                auto& binding = bindings[ShaderBindingHandle(b->name)];

                int anyNonNullIndex = 0;
                for (auto& bindingElement : binding)
                {
                    if (bindingElement.second.GetRef() != nullptr)
                    {
                        anyNonNullIndex = bindingElement.first;
                    }
                }

                switch (b->type)
                {
                    case Gfx::ShaderInfo::BindingType::UBO:
                    case Gfx::ShaderInfo::BindingType::SSBO:
                        writes[writeCount].pBufferInfo = &bufferInfos[bufferWriteIndex];
                        break;
                    case Gfx::ShaderInfo::BindingType::Texture:
                    case Gfx::ShaderInfo::BindingType::StorageImage:
                    case Gfx::ShaderInfo::BindingType::SeparateImage:
                    case Gfx::ShaderInfo::BindingType::SeparateSampler:
                        writes[writeCount].pImageInfo = &imageInfos[imageWriteIndex];
                        break;
                }

                for (int i = 0; i < writes[writeCount].descriptorCount; ++i)
                {
                    ResourceRef resRef = binding[i];

                    switch (b->type)
                    {
                        case ShaderInfo::BindingType::UBO:
                        case ShaderInfo::BindingType::SSBO:
                            {
                                VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                                VKBuffer* buffer = nullptr;
                                if (resRef.type != ShaderBindingType::Buffer || resRef.GetRef() == nullptr)
                                {
                                    std::string bufferName =
                                        fmt::format("Default Buffer for {}", shaderProgram->GetName());
                                    Buffer::CreateInfo createInfo{
                                        .usages = (b->type == ShaderInfo::BindingType::UBO ? BufferUsage::Uniform
                                                                                           : BufferUsage::Storage) |
                                                  BufferUsage::Transfer_Dst,
                                        .size = 1,
                                        .visibleInCPU = false,
                                        .debugName = bufferName.c_str()
                                    };
                                    defaultBuffer = std::make_unique<VKBuffer>(createInfo);
                                    buffer = defaultBuffer.get();
                                }
                                else
                                {
                                    buffer = (VKBuffer*)resRef.GetRef();
                                }

                                if (buffer->IsGPUWrite())
                                {
                                    VkPipelineStageFlags pipelineStages = 0;
                                    if (b->stages & Gfx::ShaderInfo::ShaderStage::Vert)
                                        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                                    if (b->stages & Gfx::ShaderInfo::ShaderStage::Frag)
                                        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                                    if (b->stages & Gfx::ShaderInfo::ShaderStage::Comp)
                                        pipelineStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Buffer,
                                        .data = buffer->GetSRef(),
                                        .stages = pipelineStages,
                                        .access = static_cast<VkAccessFlags>(
                                            VK_ACCESS_SHADER_READ_BIT |
                                            (b->type == ShaderInfo::BindingType::SSBO ? VK_ACCESS_SHADER_WRITE_BIT : 0)
                                        ),
                                    };

                                    writableGPUResources->push_back(gpuResource);
                                }
                                bufferInfo.buffer = buffer->GetHandle();
                                bufferInfo.offset = 0;
                                bufferInfo.range = VK_WHOLE_SIZE;
                                break;
                            }
                        case ShaderInfo::BindingType::StorageImage:
                            {
                                // it's possible a storage image isn't used if it's an array
                                if (resRef.GetRef() == nullptr)
                                {
                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.sampler = VK_NULL_HANDLE;
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    imageInfo.imageView =
                                        sharedResource->GetDefaultStoargeImage2D()->GetDefaultVkImageView();
                                }
                                else
                                {
                                    VKImageView* imageView = (VKImageView*)resRef.GetRef();
                                    VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b->stages);

                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Image,
                                        .data = imageView->GetImage().GetSRef(),
                                        .stages = pipelineStages,
                                        .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                        .imageView = imageView,
                                        .layout = VK_IMAGE_LAYOUT_GENERAL,
                                    };

                                    writableGPUResources->push_back(gpuResource);

                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    imageInfo.sampler = sharedResource->GetDefaultSampler();
                                    if (resRef.GetRef() != nullptr && resRef.type == ShaderBindingType::ImageView)
                                    {
                                        imageInfo.imageView = imageView->GetHandle();
                                    }
                                    else
                                    {
                                        assert(false && "a storage image has to be set before use");
                                        // imageInfo.imageView =
                                        // sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                    }
                                }

                                break;
                            }
                        case ShaderInfo::BindingType::Texture:
                        case ShaderInfo::BindingType::SeparateImage:
                            {
                                VKImageView* imageView = (VKImageView*)resRef.GetRef();
                                if (b->binding.texture.type == ShaderInfo::Texture::Type::Tex2D ||
                                    b->binding.texture.type == ShaderInfo::Texture::Type::Tex3D)
                                {
                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    imageInfo.sampler = b->type == ShaderInfo::BindingType::Texture
                                                            ? sharedResource->GetDefaultSampler()
                                                            : VK_NULL_HANDLE;
                                    if (resRef.GetRef() != nullptr &&
                                        !imageView->GetImage().GetDescription().isCubemap &&
                                        resRef.type == ShaderBindingType::ImageView)
                                    {
                                        imageInfo.imageView = imageView->GetHandle();
                                    }
                                    else
                                    {
                                        if (b->binding.texture.type == ShaderInfo::Texture::Type::Tex2D)
                                            imageInfo.imageView =
                                                sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                                        else if (b->binding.texture.type == ShaderInfo::Texture::Type::Tex3D)
                                            imageInfo.imageView =
                                                sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                    }
                                }
                                else if (b->binding.texture.type == ShaderInfo::Texture::Type::TexCube)
                                {
                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    imageInfo.sampler = sharedResource->GetDefaultSampler();

                                    if (resRef.GetRef() != nullptr &&
                                        imageView->GetImage().GetDescription().isCubemap &&
                                        resRef.type == ShaderBindingType::ImageView)
                                    {
                                        imageInfo.imageView = imageView->GetHandle();
                                    }
                                    else
                                    {
                                        imageInfo.imageView =
                                            sharedResource->GetDefaultTextureCube()->GetDefaultVkImageView();
                                    }
                                }

                                if (imageView && imageView->GetImage().IsGPUWrite())
                                {
                                    VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b->stages);
                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Image,
                                        .data = imageView->GetImage().GetSRef(),
                                        .stages = pipelineStages,
                                        .access = VK_ACCESS_SHADER_READ_BIT,
                                        .imageView = imageView,
                                        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    };

                                    writableGPUResources->push_back(gpuResource);
                                }
                                break;
                            }
                        case ShaderInfo::BindingType::SeparateSampler:
                            {
                                auto createInfo = SamplerCachePool::GenerateSamplerCreateInfoFromString(
                                    b->actualName,
                                    b->binding.separateSampler.enableCompare
                                );
                                VkSampler sampler = SamplerCachePool::RequestSampler(createInfo);
                                VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = sampler;
                                imageInfo.imageView = VK_NULL_HANDLE;
                                break;
                            }
                        default: assert(0 && "Not implemented"); break;
                    }
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
    if (iter == sets.end() || iter->second.rebuild)
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
