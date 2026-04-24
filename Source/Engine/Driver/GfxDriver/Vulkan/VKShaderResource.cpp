#include "VKShaderResource.hpp"
#include "Engine/Library/Assert.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "VKBuffer.hpp"
#include "VKCommandBufferProcessor.hpp"
#include "VKContext.hpp"
#include "VKDescriptorPool.hpp"
#include "VKDriver.hpp"
#include "VKRayTracingContext.hpp"
#include "VKShaderProgram.hpp"
#include "VKSharedResource.hpp"
#include <fmt/format.h>
#include <list>
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
    ClearAllSets();
}

VKShaderResource::VKShaderResource()
    : sharedResource(VKContext::Instance()->sharedResource), inflightSets(VKContext::Instance()->driverConfig.swapchainImageCount) {}

VKShaderResource::~VKShaderResource()
{
    ClearAllSets();
}

void VKShaderResource::RebuildAll()
{
    for (auto& sets : inflightSets)
    {
        for (auto& set : sets)
        {
            set.second.fullRebuild = true;
        }
    }
}

void VKShaderResource::ClearAllSets()
{
    for (auto& sets : inflightSets)
    {
        for (auto& d : sets)
        {
            if (d.second.descriptorPool != nullptr)
                d.second.descriptorPool->Deallocate(d.second.set);
        }

        sets.clear();
    }
}

void VKShaderResource::SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer* buffer)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != buffer)
    {
        ResourceRef ref = {ObjPtr<Buffer>(buffer), ShaderBindingType::Buffer};
        if (buffer == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = ref;

        for (auto& inflightSet : inflightSets)
        {
            for (auto& set : inflightSet)
            {
                set.second.pendingBindingUpdates.push_back({handle, index, ref});
            }
        }
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, const Gfx::ImageIdentifier& imageId)
{
    // Resolve the actual image pointer so we can skip pushing an update when the
    // underlying image hasn't changed (the render graph may recreate it between frames).
    VKImage* resolvedPtr = nullptr;
    auto idType = imageId.GetType();
    if (idType == ImageIdentifier::Type::Image)
        resolvedPtr = static_cast<VKImage*>(imageId.GetAsImage());
    else if (idType == ImageIdentifier::Type::ImageView)
        resolvedPtr = static_cast<VKImage*>(&imageId.GetAsImageView()->GetImage());
    if (idType == ImageIdentifier::Type::Handle)
    {
        if (auto* allocator = VKContext::Instance()->resourceAllocator.get())
        {
            resolvedPtr = allocator->GetImage(imageId.GetAsUUID());
        }
    }

    auto& binding = bindings[handle][index];
    if (binding.type == ShaderBindingType::ImageID && resolvedPtr != nullptr && binding.cachedResolvedDynamicImageUUID == resolvedPtr->GetUUID())
        return;

    ResourceRef ref = {imageId, ShaderBindingType::ImageID};
    if (resolvedPtr != nullptr)
        ref.cachedResolvedDynamicImageUUID = resolvedPtr->GetUUID();
    bindings[handle][index] = ref;

    for (auto& inflightSet : inflightSets)
    {
        for (auto& set : inflightSet)
        {
            set.second.pendingBindingUpdates.push_back({handle, index, ref});
        }
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, Gfx::Image* image)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != &image->GetDefaultImageViewForShaderResource())
    {
        ResourceRef ref = {
            ObjPtr<ImageView>(&image->GetDefaultImageViewForShaderResource()),
            ShaderBindingType::ImageView
        };

        if (image == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = ref;

        for (auto& inflightSet : inflightSets)
        {
            for (auto& set : inflightSet)
            {
                set.second.pendingBindingUpdates.push_back({handle, index, ref});
            }
        }
    }
}

void VKShaderResource::SetImage(ShaderBindingHandle handle, int index, Gfx::ImageView* imageView)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != imageView)
    {
        ResourceRef ref = {ObjPtr<ImageView>(imageView), ShaderBindingType::ImageView};

        if (imageView == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = ref;

        for (auto& inflightSet : inflightSets)
        {
            for (auto& set : inflightSet)
            {
                set.second.pendingBindingUpdates.push_back({handle, index, ref});
            }
        }
    }
}

void VKShaderResource::SetAccelerationStructure(ShaderBindingHandle handle, int index, RayTracingContext* context, RayTracingSceneHandle scene)
{
    auto& binding = bindings[handle][index];
    AccelerationStructureRef newRef{context, scene};
    if (binding.type != ShaderBindingType::AccelerationStructure ||
        !(std::get<AccelerationStructureRef>(binding.res) == newRef))
    {
        binding = {newRef, ShaderBindingType::AccelerationStructure};
        ResourceRef ref = {newRef, ShaderBindingType::AccelerationStructure};

        for (auto& inflightSet : inflightSets)
        {
            for (auto& set : inflightSet)
            {
                set.second.pendingBindingUpdates.push_back({handle, index, ref});
            }
        }
    }
}

void VKShaderResource::SetSampler(ShaderBindingHandle handle, int index, Gfx::Sampler* sampler)
{
    auto& binding = bindings[handle][index];
    if (binding.GetRef() != sampler)
    {
        ResourceRef ref = {ObjPtr<Gfx::Sampler>(sampler), ShaderBindingType::Sampler};
        if (sampler == nullptr)
            bindings.erase(handle);
        else
            bindings[handle][index] = ref;

        for (auto& inflightSet : inflightSets)
        {
            for (auto& set : inflightSet)
            {
                set.second.pendingBindingUpdates.push_back({handle, index, ref});
            }
        }
    }
}

void VKShaderResource::Remove(ShaderBindingHandle handle)
{
    auto it = bindings.find(handle);
    if (it != bindings.end())
    {
        ResourceRef nullRef{};
        for (auto& [index, _] : it->second)
        {
            for (auto& inflightSet : inflightSets)
            {
                for (auto& set : inflightSet)
                {
                    set.second.pendingBindingUpdates.push_back({handle, index, nullRef});
                }
            }
        }
        bindings.erase(it);
    }
}

VkDescriptorSet VKShaderResource::GetDescriptorSet(int currentInflightIndex, uint32_t set, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph)
{
    if (shaderProgram == nullptr || !shaderProgram->HasSet(set))
        return VK_NULL_HANDLE;
    SetGroup setGroup = {shaderProgram->GetUUID(), set};

    auto& sets = this->inflightSets[currentInflightIndex];

    auto setInfo = sets.find(setGroup);

    VkDescriptorSet finalReturn = VK_NULL_HANDLE;
    bool incrementalBuild = false;
    bool fullRebuild = false;
    std::vector<VKWritableGPUResource>* writableGPUResources;

    // Helper to allocate from pool with variable descriptor count support
    auto allocateFromPool = [&](VKDescriptorPool* pool) -> VkDescriptorSet
    {
        if (shaderProgram->HasVariableDescriptorCount(set))
        {
            uint32_t maxCount = shaderProgram->GetMaxVariableDescriptorCount(set);
            return pool->Allocate(maxCount);
        }
        return pool->Allocate();
    };

    if (setInfo == sets.end())
    {
        auto pool = shaderProgram->GetDescriptorPool(set);
        VkDescriptorSet descriptorSet = allocateFromPool(pool);
        finalReturn = descriptorSet;
        fullRebuild = true;
        sets[setGroup] = {shaderProgram, pool, set, finalReturn, {}, {}};
        writableGPUResources = &sets[setGroup].writableGPUResources;
    }
    else
    {
        if (setInfo->second.creationSetIndex != set)
        {
            SPDLOG_ERROR("shader resource is binded to a different set, this is not allowed");
            return VK_NULL_HANDLE;
        }
        fullRebuild = setInfo->second.fullRebuild;
        incrementalBuild = !setInfo->second.pendingBindingUpdates.empty();
        if (fullRebuild)
        {
            setInfo->second.descriptorPool->Deallocate(setInfo->second.set);
            setInfo->second.descriptorPool = shaderProgram->GetDescriptorPool(set);
            finalReturn = allocateFromPool(setInfo->second.descriptorPool);
            setInfo->second.set = finalReturn;
            setInfo->second.fullRebuild = false;
            writableGPUResources = &setInfo->second.writableGPUResources;
        }
        else if (incrementalBuild)
        {
            finalReturn = setInfo->second.set;
            writableGPUResources = &setInfo->second.writableGPUResources;
        }
        else
        {
            finalReturn = setInfo->second.set;
        }
    }

    if (incrementalBuild || fullRebuild)
    {
        // create resources and write it to descriptor set
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asWrites;
        std::vector<VkAccelerationStructureKHR> asHandles;

        auto& shaderInfo = shaderProgram->GetShaderInfo();
        const auto& descriptorSet = shaderInfo.descriptorSets[set];

        auto processResourceRef = [&](const Gfx::ShaderPipelineInfo::Binding& b, ResourceRef resRef, int bindingElementIndex)
        {
            switch (b.descriptorType)
            {
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer:
                    {
                        bufferInfos.push_back({});
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos.back();
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
                                .handle = ShaderBindingHandle(b.name),
                                .index = bindingElementIndex,
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
                            if (b.isTextureArray)
                            {
                                auto& imageView = sharedResource->GetDefaultStoargeImage2D()->GetImageView(Gfx::ImageViewOption{0, 1, 0, 1, Gfx::ImageAspect::Color, true});
                                imageInfos.push_back({});
                                VkDescriptorImageInfo& imageInfo = imageInfos.back();
                                imageInfo.sampler = VK_NULL_HANDLE;
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                imageInfo.imageView = static_cast<VKImageView&>(imageView).GetHandle();
                            }
                            else
                            {
                                imageInfos.push_back({});
                                VkDescriptorImageInfo& imageInfo = imageInfos.back();
                                imageInfo.sampler = VK_NULL_HANDLE;
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                imageInfo.imageView =
                                    sharedResource->GetDefaultStoargeImage2D()->GetDefaultVkImageView();
                            }
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
                                .handle = ShaderBindingHandle(b.name),
                                .index = bindingElementIndex,
                                .type = VKWritableGPUResource::Type::Image,
                                .data = ObjPtr<Image>(&imageView->GetImage()),
                                .stages = pipelineStages,
                                .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                .imageView = imageView,
                                .layout = VK_IMAGE_LAYOUT_GENERAL,
                            };

                            writableGPUResources->push_back(gpuResource);

                            imageInfos.push_back({});
                            VkDescriptorImageInfo& imageInfo = imageInfos.back();
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
                        {
                            auto& imageIdentifier = resRef.GetID();
                            if (imageIdentifier.GetType() == Gfx::ImageIdentifier::Type::Image)
                            {
                                imageView = static_cast<VKImageView*>(&imageIdentifier.GetAsImage()->GetDefaultImageView());
                            }
                            else
                            {
                                imageView =
                                    static_cast<VKImageView*>(&graph->GetImage(resRef.GetID().GetAsUUID())
                                                                   ->GetDefaultImageViewForShaderResource());
                            }
                        }

                        if (b.textureType == TextureType::Tex2D || b.textureType == TextureType::Tex3D)
                        {
                            imageInfos.push_back({});
                            VkDescriptorImageInfo& imageInfo = imageInfos.back();
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
                            imageInfos.push_back({});
                            VkDescriptorImageInfo& imageInfo = imageInfos.back();
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
                                .handle = ShaderBindingHandle(b.name),
                                .index = bindingElementIndex,
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
                        VkSampler vkSampler = VK_NULL_HANDLE;
                        if (resRef.type == ShaderBindingType::Sampler)
                        {
                            auto* s = static_cast<VKSampler*>(std::get<ObjPtr<Gfx::Sampler>>(resRef.res).Get());
                            if (s)
                                vkSampler = s->GetVkSampler();
                        }
                        if (vkSampler == VK_NULL_HANDLE)
                        {
                            auto createInfo = SamplerCachePool::GenerateSamplerCreateInfo(
                                descriptorSet.samplerConfigs[b.samplerIndex]
                            );
                            vkSampler = SamplerCachePool::RequestSampler(createInfo);
                        }
                        imageInfos.push_back({});
                        VkDescriptorImageInfo& imageInfo = imageInfos.back();
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.sampler = vkSampler;
                        imageInfo.imageView = VK_NULL_HANDLE;
                        break;
                    }
                case DescriptorType::AccelerationStructure:
                    {
                        if (resRef.type == ShaderBindingType::AccelerationStructure)
                        {
                            auto& asRef = std::get<AccelerationStructureRef>(resRef.res);
                            asHandles.push_back((VkAccelerationStructureKHR) static_cast<VKRayTracingContext*>(asRef.context)->GetNativeHandle(asRef.scene));
                        }
                        else
                        {
                            asHandles.push_back(VK_NULL_HANDLE);
                        }
                        break;
                    }
                default: ASSERT(0 && "Not implemented"); break;
            }
        };

        auto processWriteDescriptorSet = [&](const Gfx::ShaderPipelineInfo::Binding& b, uint32_t dstArrayElement, uint32_t descriptorCount)
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = VK_NULL_HANDLE;
            w.dstSet = finalReturn;
            w.descriptorType = MapDescriptorType(b.descriptorType);
            w.dstBinding = b.bindingNum;
            w.dstArrayElement = dstArrayElement;
            w.descriptorCount = descriptorCount;
            w.pImageInfo = VK_NULL_HANDLE;
            w.pBufferInfo = VK_NULL_HANDLE;
            w.pTexelBufferView = VK_NULL_HANDLE;

            // store index as fake pointer; patched to real pointer before vkUpdateDescriptorSets
            switch (b.descriptorType)
            {
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer:
                case DescriptorType::UniformBufferDynamic:
                case DescriptorType::StorageBufferDynamic:
                    w.pBufferInfo = (VkDescriptorBufferInfo*)(uintptr_t)bufferInfos.size();
                    break;
                case DescriptorType::CombinedImageSampler:
                case DescriptorType::StorageImage:
                case DescriptorType::SampledImage:
                case DescriptorType::UniformTexelBuffer:
                case DescriptorType::StorageTexelBuffer:
                case DescriptorType::Sampler:
                    w.pImageInfo = (VkDescriptorImageInfo*)(uintptr_t)imageInfos.size();
                    break;
                case DescriptorType::AccelerationStructure:
                    {
                        VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
                        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                        asWrite.pNext = VK_NULL_HANDLE;
                        asWrite.accelerationStructureCount = descriptorCount;
                        asWrite.pAccelerationStructures = (VkAccelerationStructureKHR*)(uintptr_t)asHandles.size();
                        asWrites.push_back(asWrite);
                        w.pNext = (void*)(uintptr_t)(asWrites.size() - 1);
                    }
                    break;
                case DescriptorType::InputAttachment:
                case DescriptorType::Invalid: break;
            }

            writes.push_back(w);
        };

        if (incrementalBuild)
        {
            for (auto& pendingBindingUpdate : setInfo->second.pendingBindingUpdates)
            {
                const Gfx::ShaderPipelineInfo::Binding* pBinding = nullptr;
                for (const auto& b : descriptorSet.bindings)
                {
                    if (ShaderBindingHandle(b.name) == pendingBindingUpdate.handle)
                    {
                        pBinding = &b;
                        break;
                    }
                }
                if (!pBinding)
                    continue;

                const auto& b = *pBinding;

                // Skip bindings with zero descriptor count (reflected but unused by shader).
                // This handles both regular bindings and variable-count bindless arrays
                // that Slang reflects with count=0 when the shader doesn't access them.
                if (b.descriptorCount == 0)
                    continue;

                // update writable GPU resources
                if (writableGPUResources)
                {
                    // remove coresponding gpuWriteResource in writableGPUResources
                    // processResourceRef will add the new one for the resource
                    for (auto& w : *writableGPUResources)
                    {
                        if (w.index == pendingBindingUpdate.elementIndex && w.handle == pendingBindingUpdate.handle)
                        {
                            std::swap(w, writableGPUResources->back());
                            writableGPUResources->pop_back();
                            break;
                        }
                    }
                }

                // update descriptor sets
                processWriteDescriptorSet(b, pendingBindingUpdate.elementIndex, 1);
                processResourceRef(b, pendingBindingUpdate.resource, pendingBindingUpdate.elementIndex);
            }

            setInfo->second.pendingBindingUpdates.clear();
        }

        if (fullRebuild)
        {
            SPDLOG_TRACE("VKShaderResource: rebuild descriptor set");
            writableGPUResources->clear();
            SetNameInternal(name, shaderProgram, finalReturn, set);

            for (const auto& b : descriptorSet.bindings)
            {
                ShaderBindingHandle nameHash(b.name);
                auto binding = bindings.find(nameHash);

                if (b.isVariableDescriptorCount)
                {
                    // For variable-count bindings, only write actually bound elements
                    // (PARTIALLY_BOUND flag covers unbound slots)
                    // Skip if descriptor count is 0 (shader doesn't use this binding)
                    if (b.descriptorCount > 0 && binding != bindings.end())
                    {
                        for (auto& [elemIndex, resRef] : binding->second)
                        {
                            processWriteDescriptorSet(b, elemIndex, 1);
                            processResourceRef(b, resRef, elemIndex);
                        }
                    }
                }
                else
                {
                    if (b.descriptorCount == 0)
                        continue;

                    processWriteDescriptorSet(b, 0, b.descriptorCount);

                    for (int i = 0; i < writes.back().descriptorCount; ++i)
                    {
                        ResourceRef resRef = binding != bindings.end() ? binding->second[i] : ResourceRef();
                        processResourceRef(b, resRef, i);
                    }
                }
            }
        }

        // patch fake-pointer indices into real pointers now that vectors are stable
        for (auto& asw : asWrites)
            asw.pAccelerationStructures = asHandles.data() + (uintptr_t)asw.pAccelerationStructures;
        for (auto& w : writes)
        {
            switch (w.descriptorType)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    w.pBufferInfo = bufferInfos.data() + (uintptr_t)w.pBufferInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    w.pImageInfo = imageInfos.data() + (uintptr_t)w.pImageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    w.pNext = asWrites.data() + (uintptr_t)w.pNext;
                    break;
                default: break;
            }
        }
        vkUpdateDescriptorSets(GetDevice(), (uint32_t)writes.size(), writes.data(), 0, VK_NULL_HANDLE);
    }

    return finalReturn;
}

void VKShaderResource::SetName(std::string_view name)
{
    this->name = name;
    for (auto& s : inflightSets)
    {
        for (auto& d : s)
        {
            SetNameInternal(name, d.second.program, d.second.set, d.second.creationSetIndex);
        }
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
    else if (type == ShaderBindingType::Sampler)
        return std::get<ObjPtr<Gfx::Sampler>>(res).Get();
    // not doing for ImageID because the underlying image may change

    return nullptr;
}

const std::vector<VKWritableGPUResource>& VKShaderResource::GetWritableResources(int inflightIndex, uint32_t set, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph)
{
    SetGroup setGroup = {shaderProgram->GetUUID(), set};
    auto& sets = inflightSets[inflightIndex];
    auto iter = sets.find(setGroup);
    if (iter == sets.end() || !iter->second.pendingBindingUpdates.empty() || iter->second.fullRebuild)
    {
        GetDescriptorSet(inflightIndex, set, shaderProgram, graph);
        iter = sets.find(setGroup);
    }

    if (iter != sets.end() && iter->second.creationSetIndex == set)
    {
        return iter->second.writableGPUResources;
    }

    static std::vector<VKWritableGPUResource> empty;
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
