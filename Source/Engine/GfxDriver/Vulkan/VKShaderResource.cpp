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

namespace Engine::Gfx
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

VKShaderResource::VKShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency)
    : ShaderResource(frequency), shaderProgram(static_cast<VKShaderProgram*>(shader.Get())),
      sharedResource(VKContext::Instance()->sharedResource), device(VKContext::Instance()->device)
{
    slot = MapDescriptorSetSlot(frequency);
    descriptorPool = &shaderProgram->GetDescriptorPool(slot);
    descriptorSet = descriptorPool->Allocate();
    auto& shaderInfo = shaderProgram->GetShaderInfo();
    VKDebugUtils::SetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSet, std::to_string(slot).c_str());

    // allocate push constant buffer
    uint32_t pushConstantSize = 0;
    for (auto& ps : shaderInfo.pushConstants)
    {
        pushConstantSize += ps.second.data.size;
    }
    if (pushConstantSize != 0)
        pushConstantBuffer = new unsigned char[pushConstantSize];

    // create resources and write it to descriptor set
    VkWriteDescriptorSet writes[32];
    VkDescriptorBufferInfo bufferInfos[32];
    VkDescriptorImageInfo imageInfos[32];
    uint32_t bufferWriteIndex = 0;
    uint32_t imageWriteIndex = 0;
    uint32_t writeCount = 0;
    auto iter = shaderInfo.descriptorSetBinidngMap.find(slot);
    if (iter != shaderInfo.descriptorSetBinidngMap.end())
    {
        assert(iter->second.bindings.size() < 64);
        for (auto& b : iter->second.bindings)
        {
            writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[writeCount].pNext = VK_NULL_HANDLE;
            writes[writeCount].dstSet = descriptorSet;
            writes[writeCount].descriptorType = ShaderInfo::Utils::MapBindingType(b.second.type);
            writes[writeCount].dstBinding = b.second.bindingNum;
            writes[writeCount].dstArrayElement = 0;
            writes[writeCount].descriptorCount = b.second.count;
            writes[writeCount].pImageInfo = VK_NULL_HANDLE;
            writes[writeCount].pBufferInfo = VK_NULL_HANDLE;
            writes[writeCount].pTexelBufferView = VK_NULL_HANDLE;
            switch (b.second.type)
            {
                case ShaderInfo::BindingType::UBO:
                    {
                        for (int i = 0; i < writes[writeCount].descriptorCount; ++i)
                        {
                            std::string bufferName = fmt::format("{}_UBO", shader->GetName());
                            Buffer::CreateInfo createInfo{
                                .usages = BufferUsage::Uniform | BufferUsage::Transfer_Dst,
                                .size = b.second.binding.ubo.data.size,
                                .visibleInCPU = false,
                                .debugName = bufferName.c_str()};
                            auto buffer = MakeUnique<VKBuffer>(createInfo);
                            VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                            bufferInfo.buffer = buffer->GetHandle();
                            bufferInfo.offset = i * b.second.binding.ubo.data.size;
                            bufferInfo.range = b.second.binding.ubo.data.size;
                            writes[writeCount].pBufferInfo = &bufferInfo;
                            uniformBuffers.insert(std::make_pair(b.second.name, std::move(buffer)));
                        }
                        break;
                    }
                case ShaderInfo::BindingType::SSBO:
                    writeCount--; // no default SSBO yet
                    break;
                //{
                //     VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                //     bufferInfo.buffer = sharedResource->GetDefaultStorageBuffer()->GetVKBuffer();
                //     bufferInfo.offset = 0;
                //     bufferInfo.range = b.second.binding.ubo.data.size;
                //     writes[writeCount].pBufferInfo = &bufferInfo;
                //     storageBuffers.insert(std::make_pair(b.second.name, sharedResource->GetDefaultStorageBuffer()));
                //     break;
                // }
                case ShaderInfo::BindingType::Texture:
                    {
                        if (b.second.binding.texture.type == ShaderInfo::Texture::Type::Tex2D)
                        {
                            VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.sampler = sharedResource->GetDefaultSampler();
                            imageInfo.imageView = sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                            writes[writeCount].pImageInfo = &imageInfo;
                            imageVies["_default_uTexutre2D"] =
                                static_cast<VKImageView*>(&sharedResource->GetDefaultTexture2D()->GetDefaultImageView()
                                );
                        }
                        else if (b.second.binding.texture.type == ShaderInfo::Texture::Type::TexCube)
                        {
                            VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.sampler = sharedResource->GetDefaultSampler();
                            imageInfo.imageView = sharedResource->GetDefaultTextureCube()->GetDefaultVkImageView();
                            writes[writeCount].pImageInfo = &imageInfo;
                            imageVies["_default_uTexutreCube"] = static_cast<VKImageView*>(
                                &sharedResource->GetDefaultTextureCube()->GetDefaultImageView()
                            );
                        }
                        break;
                    }
                case ShaderInfo::BindingType::SeparateImage:
                    {
                        VkDescriptorImageInfo& imageInfo = imageInfos[imageWriteIndex++];
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.sampler = VK_NULL_HANDLE;
                        imageInfo.imageView = sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                        writes[writeCount].pImageInfo = &imageInfo;
                        imageVies["_default_uTexutreSeparateImage"] =
                            static_cast<VKImageView*>(&sharedResource->GetDefaultTexture2D()->GetDefaultImageView());
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

    vkUpdateDescriptorSets(device->GetHandle(), writeCount, writes, 0, VK_NULL_HANDLE);
}

VKShaderResource::~VKShaderResource()
{
    if (pushConstantBuffer != nullptr)
        delete pushConstantBuffer;
    descriptorPool->Free(descriptorSet);
}

bool VKShaderResource::HasPushConstnat(const std::string& obj)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();
    // find from push constant first
    auto pushConstantIter = shaderInfo.pushConstants.find(obj);
    if (pushConstantIter != shaderInfo.pushConstants.end())
    {
        return true;
    }

    return false;
}

RefPtr<Buffer> VKShaderResource::GetBuffer(const std::string& object, BufferMemberInfoMap& memberInfo)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();

    // find binding
    auto& bindings = shaderInfo.descriptorSetBinidngMap.at(slot).bindings;
    auto iter = bindings.find(object);
    if (iter == bindings.end())
    {
        SPDLOG_WARN("SetUniform didn't find the binding");
        return nullptr;
    }
    auto& binding = iter->second;

    // ensure binding is ubo
    if (binding.type != ShaderInfo::BindingType::UBO)
    {
        SPDLOG_WARN("SetUniform is writing to a non-uniform binding");
        return nullptr;
    }

    // get buffer
    auto bufIter = uniformBuffers.find(binding.name);
    if (bufIter == uniformBuffers.end())
        return nullptr;

    // write to buffer
    auto& mems = binding.binding.ubo.data.members;
    for (auto& m : mems)
    {
        memberInfo[m.first] = {m.second.offset, m.second.data->size};
    }

    return bufIter->second.Get();
}

VkDescriptorSet VKShaderResource::GetDescriptorSet()
{
    return descriptorSet;
}

void VKShaderResource::SetImage(const std::string& param, RefPtr<Image> image)
{
    SetImage(param, &image->GetDefaultImageView());
}

void VKShaderResource::SetBuffer(const std::string& bindingName, Buffer& buffer)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();
    auto& bindings = shaderInfo.descriptorSetBinidngMap.at(slot).bindings;
    auto iter = bindings.find(bindingName);

    if (iter == bindings.end())
    {
        return;
    }

    if ((iter->second.type == ShaderInfo::BindingType::UBO && !HasFlag(buffer.GetUsages(), BufferUsage::Uniform)) ||
        (iter->second.type == ShaderInfo::BindingType::SSBO && !HasFlag(buffer.GetUsages(), BufferUsage::Storage)))
    {
        return;
    }

    VkWriteDescriptorSet write;
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = VK_NULL_HANDLE;
    write.dstSet = descriptorSet;
    write.descriptorType = ShaderInfo::Utils::MapBindingType(iter->second.type);
    write.dstBinding = iter->second.bindingNum;
    write.dstArrayElement = 0;
    write.descriptorCount = iter->second.count;
    write.pImageInfo = VK_NULL_HANDLE;
    write.pBufferInfo = VK_NULL_HANDLE;
    write.pTexelBufferView = VK_NULL_HANDLE;

    VkDescriptorBufferInfo bufferInfo{.buffer = ((VKBuffer*)&buffer)->GetHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device->GetHandle(), 1, &write, 0, VK_NULL_HANDLE);
}

/* void VKShaderResource::SetStorage(std::string_view param, RefPtr<StorageBuffer> storage)
 {
     std::string paramStr(param);
     VKStorageBuffer* storageBuffer = static_cast<VKStorageBuffer*>(storage.Get());
     VkDescriptorBufferInfo bufferInfo;
     bufferInfo.buffer = storageBuffer->GetVKBuffer();
     bufferInfo.offset = 0;
     bufferInfo.range = storageBuffer->GetSize();

     auto& shaderInfo = shaderProgram->GetShaderInfo();
     auto iter = shaderInfo.bindings.find(paramStr);
     if (iter == shaderInfo.bindings.end())
     {
         SPDLOG_ERROR("Failed to find storage binding");
         return;
     }

     VkWriteDescriptorSet write;
     write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
     write.pNext = VK_NULL_HANDLE;
     write.dstSet = descriptorSet;
     write.descriptorType = ShaderInfo::Utils::MapBindingType(iter->second.type);
     write.dstBinding = iter->second.bindingNum;
     write.dstArrayElement = 0;
     write.descriptorCount = iter->second.count;
     write.pImageInfo = VK_NULL_HANDLE;
     write.pBufferInfo = VK_NULL_HANDLE;
     write.pTexelBufferView = VK_NULL_HANDLE;
     vkUpdateDescriptorSets(device->GetHandle(), 1, &write, 0, VK_NULL_HANDLE);

     storageBuffers[paramStr] = storage;
 }*/

RefPtr<ShaderProgram> VKShaderResource::GetShaderProgram()
{
    return shaderProgram.Get();
}

void VKShaderResource::SetImage(const std::string& param, ImageView* imageView)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();

    // find the binding
    auto iter = std::find_if(
        shaderInfo.bindings.begin(),
        shaderInfo.bindings.end(),
        [&param](auto& pair) { return pair.second.name == param; }
    );
    if (iter == shaderInfo.bindings.end())
    {
        SPDLOG_WARN("SetTexture didn't find the texture in binding");
        return;
    }

    // ensure the binding is a texutre
    if (iter->second.type != ShaderInfo::BindingType::Texture &&
        iter->second.type != ShaderInfo::BindingType::SeparateImage)
    {
        SPDLOG_WARN("Setting a image to a non texture binding");
        return;
    }

    // null image use default
    if (imageView == nullptr)
    {
        imageView = &VKContext::Instance()->sharedResource->GetDefaultTexture2D()->GetDefaultImageView();
    }

    // check if this image already sets to binding
    auto texIter = imageVies.find(param);
    if (texIter != imageVies.end() && texIter->second == imageView)
    {
        return;
    }
    imageVies[param] = (VKImageView*)imageView;

    ShaderInfo::BindingType descriptorType = iter->second.type;
    uint32_t bindingNum = iter->second.bindingNum;
    uint32_t count = iter->second.count;
    VkDevice vkDevice = device->GetHandle();

    // actually set the image to binding
    VkWriteDescriptorSet write;
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = VK_NULL_HANDLE;
    write.dstSet = descriptorSet;
    write.descriptorType = ShaderInfo::Utils::MapBindingType(descriptorType);
    write.dstBinding = bindingNum;
    write.dstArrayElement = 0;
    write.descriptorCount = count;
    write.pImageInfo = VK_NULL_HANDLE;
    write.pBufferInfo = VK_NULL_HANDLE;
    write.pTexelBufferView = VK_NULL_HANDLE;
    VkDescriptorImageInfo imageInfo;
    switch (descriptorType)
    {
        case ShaderInfo::BindingType::Texture:
            {
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.sampler = VKContext::Instance()->sharedResource->GetDefaultSampler(
                ); // TODO we need to get sampler from
                   // image(or a type called Texture maybe?)
                imageInfo.imageView = ((VKImageView*)imageView)->GetHandle();
                write.pImageInfo = &imageInfo;
                break;
            }
        case ShaderInfo::BindingType::SeparateImage:
            {
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.sampler = VK_NULL_HANDLE;
                imageInfo.imageView = ((VKImageView*)imageView)->GetHandle();
                write.pImageInfo = &imageInfo;
                break;
            }
        default: assert(0 && "Wrong type");
    }
    vkUpdateDescriptorSets(vkDevice, 1, &write, 0, VK_NULL_HANDLE);
}

void VKShaderResource::SetBuffer(Buffer& buffer, unsigned int binding, size_t offset, size_t range)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();

    auto iter = std::find_if(
        shaderInfo.bindings.begin(),
        shaderInfo.bindings.end(),
        [binding](const ShaderInfo::Bindings::value_type& p)
        {
            return p.second.bindingNum == binding &&
                   (p.second.type == ShaderInfo::BindingType::UBO || p.second.type == ShaderInfo::BindingType::SSBO);
        }
    );

    if (iter != shaderInfo.bindings.end())
    {
        auto& b = iter->second;
        VkWriteDescriptorSet write;
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = VK_NULL_HANDLE;
        write.dstSet = descriptorSet;
        write.descriptorType = ShaderInfo::Utils::MapBindingType(b.type);
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorCount = b.count;
        write.pImageInfo = VK_NULL_HANDLE;
        write.pTexelBufferView = VK_NULL_HANDLE;

        VKBuffer* buf = static_cast<VKBuffer*>(&buffer);
        VkDescriptorBufferInfo bufferInfo{.buffer = buf->GetHandle(), .offset = 0, .range = buffer.GetSize()};
        bufferInfo.offset = offset;
        bufferInfo.range = range == 0 ? buffer.GetSize() : range;
        write.pBufferInfo = &bufferInfo;

        VkDevice vkDevice = device->GetHandle();
        vkUpdateDescriptorSets(vkDevice, 1, &write, 0, VK_NULL_HANDLE);
    }
}
} // namespace Engine::Gfx
