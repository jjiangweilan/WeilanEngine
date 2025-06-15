#include "VKShaderResource.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Libs/Assert.hpp"
#include "RHI/VKRenderGraph.hpp"
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
static VkPipelineStageFlags ShaderStageToPipelineStage(ShaderStage stages)
{
    VkPipelineStageFlags pipelineStages = 0;
    if (HasFlag(stages, ShaderStage::Vertex))
        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (HasFlag(stages, ShaderStage::Fragment))
        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (HasFlag(stages, ShaderStage::Compute))
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

void VKShaderResource::Clear()
{
    bindings.clear();
    sets.clear();
}

VKShaderResource::VKShaderResource() : sharedResource(VKContext::Instance()->sharedResource), sets() {}

VKShaderResource::~VKShaderResource()
{
    for (auto& d : sets)
    {
        if (d.second.descriptorPool != nullptr)
            d.second.descriptorPool->Deallocate(d.second.set);
    }
}

void VKShaderResource::RebuildAll()
{
    sets.clear();
}

void VKShaderResource::SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != buffer)
    {
        if (buffer == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = {ObjPtr<Buffer>(buffer), ShaderBindingType::Buffer};
        RebuildAll();
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, const Gfx::RG::ImageIdentifier& imageId)
{
    auto& binding = bindings[handle][index];
    if (binding.GetID() != imageId)
    {
        bindings[handle][index] = {imageId, ShaderBindingType::ImageID};
        RebuildAll();
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, Gfx::Image* image)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != &image->GetDefaultImageViewForShaderResource())
    {
        if (image == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = {
                ObjPtr<ImageView>(&image->GetDefaultImageViewForShaderResource()),
                ShaderBindingType::ImageView
            };
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
            bindings[handle][index] = {ObjPtr<ImageView>(imageView), ShaderBindingType::ImageView};
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

VkDescriptorSet VKShaderResource::GetDescriptorSet(
    uint32_t set, VKShaderProgram* shaderProgram, VK::RenderGraph::Graph* graph
)
{
    if (shaderProgram == nullptr || !shaderProgram->HasSet(set))
        return VK_NULL_HANDLE;
    SetGroup setGroup = {shaderProgram->GetUUID(), set};

    auto iter = sets.find(setGroup);

    VkDescriptorSet finalReturn = VK_NULL_HANDLE;
    bool rebuild = false;
    DynamicArray<VKWritableGPUResource>* writableGPUResources;
    if (iter == sets.end())
    {
        auto pool = shaderProgram->GetDescriptorPool(set);
        VkDescriptorSet descriptorSet = pool->Allocate();
        finalReturn = descriptorSet;
        rebuild = true;
        sets[setGroup] = {shaderProgram, pool, set, finalReturn, false};
        writableGPUResources = &sets[setGroup].writableGPUResources;
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
            iter->second.descriptorPool->Deallocate(iter->second.set);
            iter->second.descriptorPool = shaderProgram->GetDescriptorPool(set);
            finalReturn = iter->second.descriptorPool->Allocate();
            iter->second.set = finalReturn;
            iter->second.rebuild = false;
            writableGPUResources = &iter->second.writableGPUResources;
        }
        else
        {
            finalReturn = iter->second.set;
        }
    }

    if (rebuild)
    {
        SPDLOG_TRACE("VKShaderResource: rebuild descriptor set");
        writableGPUResources->clear();
        auto& shaderInfo = shaderProgram->GetShaderInfo();
        SetNameInternal(name, shaderProgram, finalReturn, set);

        // create resources and write it to descriptor set
        VkWriteDescriptorSet writes[32];
        VkDescriptorBufferInfo bufferInfos[32];
        VkDescriptorImageInfo imageInfos[32];
        uint32_t bufferWriteIndex = 0;
        uint32_t imageWriteIndex = 0;
        uint32_t writeCount = 0;

        const auto& descriptorSet = shaderInfo.descriptorSets[set];
        {
            for (const auto& b : descriptorSet.bindings)
            {
                writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[writeCount].pNext = VK_NULL_HANDLE;
                writes[writeCount].dstSet = finalReturn;
                writes[writeCount].descriptorType = MapDescriptorType(b.descriptorType);
                writes[writeCount].dstBinding = b.bindingNum;
                writes[writeCount].dstArrayElement = 0;
                writes[writeCount].descriptorCount = b.descriptorCount;
                writes[writeCount].pImageInfo = VK_NULL_HANDLE;
                writes[writeCount].pBufferInfo = VK_NULL_HANDLE;
                writes[writeCount].pTexelBufferView = VK_NULL_HANDLE;

                ShaderBindingHandle nameHash(b.name);
                auto binding = bindings.find(nameHash);

                if (binding != bindings.end())
                {
                    int anyNonNullIndex = 0;
                    for (auto& bindingElement : binding->second)
                    {
                        if (bindingElement.second.GetRef() != nullptr)
                        {
                            anyNonNullIndex = bindingElement.first;
                        }
                    }
                }

                switch (b.descriptorType)
                {
                    case DescriptorType::UniformBuffer:
                    case DescriptorType::StorageBuffer:
                        writes[writeCount].pBufferInfo = &bufferInfos[bufferWriteIndex];
                        break;
                    case DescriptorType::CombinedImageSampler:
                    case DescriptorType::StorageImage:
                    case DescriptorType::SampledImage:
                    case DescriptorType::Sampler: writes[writeCount].pImageInfo = &imageInfos[imageWriteIndex]; break;
                }

                for (int i = 0; i < writes[writeCount].descriptorCount; ++i)
                {
                    ResourceRef resRef = binding != bindings.end() ? binding->second[i] : ResourceRef();

                    switch (b.descriptorType)
                    {
                        case DescriptorType::UniformBuffer:
                        case DescriptorType::StorageBuffer:
                            {
                                VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                                VKBuffer* buffer = nullptr;
                                if (resRef.type != ShaderBindingType::Buffer || resRef.GetRef() == nullptr)
                                {
                                    if (defaultBuffer == nullptr)
                                    {
                                        std::string bufferName =
                                            fmt::format("Default Buffer for {}", shaderProgram->GetName());
                                        Buffer::CreateInfo createInfo{
                                            .usages = (b.descriptorType == DescriptorType::UniformBuffer
                                                           ? BufferUsage::Uniform
                                                           : BufferUsage::Storage) |
                                                      BufferUsage::Transfer_Dst,
                                            .size = 1,
                                            .visibleInCPU = false,
                                            .debugName = bufferName.c_str()
                                        };
                                        defaultBuffer = std::make_unique<VKBuffer>(createInfo);
                                    }

                                    buffer = defaultBuffer.get();
                                }
                                else
                                {
                                    buffer = (VKBuffer*)resRef.GetRef();
                                }

                                if (buffer->IsGPUWrite())
                                {
                                    VkPipelineStageFlags pipelineStages = 0;
                                    if (HasFlag(b.stages, ShaderStage::Vertex))
                                        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                                    if (HasFlag(b.stages, ShaderStage::Fragment))
                                        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                                    if (HasFlag(b.stages, ShaderStage::Compute))
                                        pipelineStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Buffer,
                                        .data = ObjPtr<Buffer>(buffer),
                                        .stages = pipelineStages,
                                        .access = static_cast<VkAccessFlags>(
                                            VK_ACCESS_SHADER_READ_BIT |
                                            (b.descriptorType == DescriptorType::StorageBuffer
                                                 ? VK_ACCESS_SHADER_WRITE_BIT
                                                 : 0)
                                        ),
                                    };

                                    writableGPUResources->push_back(gpuResource);
                                }
                                bufferInfo.buffer = buffer->GetHandle();
                                bufferInfo.offset = 0;
                                bufferInfo.range = VK_WHOLE_SIZE;
                                break;
                            }
                        case DescriptorType::StorageImage:
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
                                    VKImageView* imageView = nullptr;
                                    if (resRef.IsImageView())
                                        imageView = (VKImageView*)resRef.GetRef();
                                    else
                                        imageView =
                                            static_cast<VKImageView*>(&graph->GetImage(resRef.GetID().GetAsUUID())
                                                                           ->GetDefaultImageViewForShaderResource());
                                    VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);

                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Image,
                                        .data = ObjPtr<Image>(&imageView->GetImage()),
                                        .stages = pipelineStages,
                                        .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                        .imageView = imageView,
                                        .layout = VK_IMAGE_LAYOUT_GENERAL,
                                    };

                                    writableGPUResources->push_back(gpuResource);

                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    imageInfo.sampler = sharedResource->GetDefaultSampler();
                                    if (resRef.GetRef() != nullptr && (resRef.type == ShaderBindingType::ImageView))
                                    {
                                        imageInfo.imageView = imageView->GetHandle();
                                    }
                                    else
                                    {
                                        // using ImageID is not supported in shader resource because we don't have the chance to know if the underlying image is changed in shader resource
                                        throw std::runtime_error("a storage image has to be set before use");
                                        // imageInfo.imageView =
                                        // sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                    }
                                }

                                break;
                            }
                        case DescriptorType::CombinedImageSampler:
                        case DescriptorType::SampledImage:
                            {
                                VKImageView* imageView = nullptr;
                                if (resRef.IsImageView())
                                    imageView = (VKImageView*)resRef.GetRef();
                                else if (resRef.IsValidRef())
                                    imageView =
                                        static_cast<VKImageView*>(&graph->GetImage(resRef.GetID().GetAsUUID())
                                                                       ->GetDefaultImageViewForShaderResource());
                                if (b.textureType == TextureType::Tex2D || b.textureType == TextureType::Tex3D)
                                {
                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    imageInfo.sampler = b.descriptorType == DescriptorType::SampledImage
                                                            ? sharedResource->GetDefaultSampler()
                                                            : VK_NULL_HANDLE;
                                    if (imageView != nullptr)
                                    {
                                        imageInfo.imageView = imageView->GetHandle();
                                    }
                                    else
                                    {
                                        if (b.textureType == TextureType::Tex2D)
                                            imageInfo.imageView =
                                                sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                                        else if (b.textureType == TextureType::Tex3D)
                                            imageInfo.imageView =
                                                sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                    }
                                }
                                else if (b.textureType == TextureType::TexCube)
                                {
                                    VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                    imageInfo.sampler = sharedResource->GetDefaultSampler();

                                    if (imageView != nullptr && imageView->GetImage().GetDescription().isCubemap)
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
                                    VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);
                                    VKWritableGPUResource gpuResource{
                                        .type = VKWritableGPUResource::Type::Image,
                                        .data = ObjPtr<Image>(&imageView->GetImage()),
                                        .stages = pipelineStages,
                                        .access = VK_ACCESS_SHADER_READ_BIT,
                                        .imageView = imageView,
                                        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    };

                                    writableGPUResources->push_back(gpuResource);
                                }
                                break;
                            }
                        case DescriptorType::Sampler:
                            {
                                auto createInfo = SamplerCachePool::GenerateSamplerCreateInfo(
                                    descriptorSet.samplerConfigs[b.samplerIndex]
                                );
                                VkSampler sampler = SamplerCachePool::RequestSampler(createInfo);
                                VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = sampler;
                                imageInfo.imageView = VK_NULL_HANDLE;
                                break;
                            }
                        default: ASSERT(0 && "Not implemented"); break;
                    }
                }

                writeCount += 1;
            }
        }

        vkUpdateDescriptorSets(GetDevice(), writeCount, writes, 0, VK_NULL_HANDLE);
    }

    return finalReturn;
}

void VKShaderResource::SetName(std::string_view name)
{
    this->name = name;
    for (auto& s : sets)
    {
        SetNameInternal(name, s.second.program, s.second.set, s.second.creationSetIndex);
    }
}

void VKShaderResource::SetNameInternal(
    std::string_view name, VKShaderProgram* shader, VkDescriptorSet set, int setIndex
)
{
    VKDebugUtils::SetDebugName(
        VK_OBJECT_TYPE_DESCRIPTOR_SET,
        (uint64_t)set,
        fmt::format("{:x}, {}, set {}", (uint64_t)set, shader->GetName(), setIndex).c_str()
    );
}

void* VKShaderResource::ResourceRef::GetRef()
{
    if (type == ShaderBindingType::Buffer)
        return std::get<ObjPtr<Buffer>>(res).Get();
    else if (type == ShaderBindingType::ImageView)
        return std::get<ObjPtr<ImageView>>(res).Get();
    // not doing for ImageID because the underlying image may change

    return nullptr;
}

const DynamicArray<VKWritableGPUResource>& VKShaderResource::GetWritableResources(
    uint32_t set, VKShaderProgram* shaderProgram, VK::RenderGraph::Graph* graph
)
{
    SetGroup setGroup = {shaderProgram->GetUUID(), set};
    auto iter = sets.find(setGroup);
    if (iter == sets.end() || iter->second.rebuild)
    {
        GetDescriptorSet(set, shaderProgram, graph);
        iter = sets.find(setGroup);
    }

    if (iter != sets.end() && iter->second.creationSetIndex == set)
    {
        return iter->second.writableGPUResources;
    }

    static DynamicArray<VKWritableGPUResource> empty;
    return empty;
}

size_t VKShaderResource::SetGroupHash::operator()(const SetGroup& group) const
{
    uint64_t hash = 0;
    Hash64(hash, group.set);
    Hash64(hash, std::hash<UUID>()(group.id));
    return hash;
}
} // namespace Gfx
