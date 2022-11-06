#include "VKShaderResource.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKShaderProgram.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKDevice.hpp"
#include "VKBuffer.hpp"
#include "VKSharedResource.hpp"
#include "VKDescriptorPool.hpp"
#include "VKDriver.hpp"
#include <spdlog/spdlog.h>
namespace Engine::Gfx
{
    DescriptorSetSlot MapDescriptorSetSlot(ShaderResourceFrequency frequency)
    {
        switch (frequency)
        {
            case ShaderResourceFrequency::Global:
                return Global_Descriptor_Set;
            case ShaderResourceFrequency::Shader:
                return Shader_Descriptor_Set;
            case ShaderResourceFrequency::Material:
                return Material_Descriptor_Set;
            case ShaderResourceFrequency::Object:
                return Object_Descriptor_Set;
            default:
                return Material_Descriptor_Set;
        }
    }

    VKShaderResource::VKShaderResource(RefPtr<ShaderProgram> shader, ShaderResourceFrequency frequency) : shaderProgram(static_cast<VKShaderProgram*>(shader.Get())), sharedResource(VKContext::Instance()->sharedResource), device(VKContext::Instance()->device)
    {
        slot = MapDescriptorSetSlot(frequency);
        descriptorPool = &shaderProgram->GetDescriptorPool(MapDescriptorSetSlot(frequency));
        descriptorSet = descriptorPool->Allocate();
        auto& shaderInfo = shaderProgram->GetShaderInfo();

        // allocate push constant buffer
        uint32_t pushConstantSize = 0;
        for (auto& ps : shaderInfo.pushConstants)
        {
            pushConstantSize += ps.second.data.size;
        }
        if (pushConstantSize != 0)
            pushConstantBuffer = new unsigned char[pushConstantSize];

        // create resources and write it to descriptor set
        VkWriteDescriptorSet writes[64];
        VkDescriptorBufferInfo bufferInfos[64]; uint32_t bufferWriteIndex = 0;
        uint32_t writeCount = 0;
        auto iter = shaderInfo.descriptorSetBinidngMap.find(slot);
        if (iter != shaderInfo.descriptorSetBinidngMap.end())
        {
            assert(iter->second.bindings.size() < 64);
            for(auto& b : iter->second.bindings)
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
                        auto buffer = Engine::MakeUnique<VKBuffer>(b.second.binding.ubo.data.size, BufferUsage::Uniform, false);
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                        bufferInfo.buffer = buffer->GetVKBuffer();
                        bufferInfo.offset = 0;
                        bufferInfo.range = b.second.binding.ubo.data.size;
                        writes[writeCount].pBufferInfo = &bufferInfo;
                        uniformBuffers.insert(std::make_pair(b.second.name, std::move(buffer)));
                        break;
                    }
                    case ShaderInfo::BindingType::SSBO:
                    {
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos[bufferWriteIndex++];
                        bufferInfo.buffer = sharedResource->GetDefaultStorageBuffer()->GetVKBuffer();
                        bufferInfo.offset = 0;
                        bufferInfo.range = b.second.binding.ubo.data.size;
                        writes[writeCount].pBufferInfo = &bufferInfo;
                        storageBuffers.insert(std::make_pair(b.second.name, sharedResource->GetDefaultStorageBuffer()));
                        break;
                    }
                    case ShaderInfo::BindingType::Texture:
                    {
                        VkDescriptorImageInfo imageInfo;
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.sampler = sharedResource->GetDefaultSampler();
                        imageInfo.imageView = sharedResource->GetDefaultTexture()->GetDefaultImageView();
                        writes[writeCount].pImageInfo = &imageInfo;
                        textures["_default_uTexutre"] = sharedResource->GetDefaultTexture();
                        break;
                    }
                    case ShaderInfo::BindingType::SeparateImage:
                    {
                        VkDescriptorImageInfo imageInfo;
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.sampler = VK_NULL_HANDLE;
                        imageInfo.imageView = sharedResource->GetDefaultTexture()->GetDefaultImageView();
                        writes[writeCount].pImageInfo = &imageInfo;
                        textures["_default_uTexutre"] = sharedResource->GetDefaultTexture();
                        break;
                    }
                    case ShaderInfo::BindingType::SeparateSampler:
                    {
                        VkDescriptorImageInfo imageInfo;
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.sampler = sharedResource->GetDefaultSampler();
                        imageInfo.imageView = VK_NULL_HANDLE;
                        writes[writeCount].pImageInfo = &imageInfo;
                        break;
                    }
                    default:
                        assert(0 && "Not implemented");
                        break;
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

    void VKShaderResource::SetUniform(std::string_view obj, std::string_view member, void* value)
    {
        std::string objStr(obj);
        std::string memStr(member);
        auto& shaderInfo = shaderProgram->GetShaderInfo();

        // find from push constant first
        auto pushConstantIter = shaderInfo.pushConstants.find(objStr);
        if (pushConstantIter != shaderInfo.pushConstants.end())
        {
            auto memIter = pushConstantIter->second.data.members.find(memStr);
            if (memIter != pushConstantIter->second.data.members.end())
            {
                pushConstantIsUsed = true;
                memcpy(pushConstantBuffer + memIter->second.offset, value, memIter->second.data->size);
                return;
            }
        }

        // find binding
        auto& bindings = shaderInfo.descriptorSetBinidngMap.at(slot).bindings;
        auto iter = bindings.find(objStr);
        if (iter == bindings.end())
        {
            SPDLOG_WARN("SetUniform didn't find the binding");
            return;
        }
        auto& binding = iter->second;

        // ensure binding is ubo
        if (binding.type != ShaderInfo::BindingType::UBO)
        {
            SPDLOG_WARN("SetUniform is writing to a non-uniform binding");
            return;
        }

        // get buffer
        auto bufIter = uniformBuffers.find(binding.name);
        if (bufIter == uniformBuffers.end()) return;
        auto& buf = bufIter->second;

        // write to buffer
        auto& mems = binding.binding.ubo.data.members;
        auto memIter = mems.find(memStr);
        if (memIter == mems.end())
        {
            SPDLOG_WARN("SetUniform didn't find the binding member");
            return;
        }
        buf->Write(value, memIter->second.data->size, memIter->second.offset);
    }

    VkDescriptorSet VKShaderResource::GetDescriptorSet()
    {
        return descriptorSet;
    }

    void VKShaderResource::SetTexture(const std::string& param, RefPtr<Image> image)
    {
        auto& shaderInfo = shaderProgram->GetShaderInfo();

        // find the binding
        auto iter = shaderInfo.bindings.find(param);
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

        // check if this image already sets to binding
        auto texIter = textures.find(param);
        if (texIter != textures.end() && texIter->second.Get() == image.Get())
        {
            return;
        }
        textures[param] = (VKImage*)image.Get();

        ShaderInfo::BindingType descriptorType = iter->second.type;
        uint32_t bindingNum = iter->second.bindingNum;
        uint32_t count = iter->second.count;
        VkDevice vkDevice = device->GetHandle();
        pendingTextureUpdates.push_back([descriptorType, bindingNum, count, image, vkDevice, descriptorSet = this->descriptorSet]()
        {
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
            imageInfo.sampler = VKContext::Instance()->sharedResource->GetDefaultSampler(); // TODO we need to get sampler from image(or a type called Texture maybe?)
        imageInfo.imageView = ((VKImage*)image.Get())->GetDefaultImageView();
        write.pImageInfo = &imageInfo;
        break;
            }
            case ShaderInfo::BindingType::SeparateImage:
            {
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.sampler = VK_NULL_HANDLE;
                imageInfo.imageView = ((VKImage*)image.Get())->GetDefaultImageView();
                write.pImageInfo = &imageInfo;
                break;
            }
            default:
            assert(0 && "Wrong type");
            }
            vkUpdateDescriptorSets(vkDevice, 1, &write, 0, VK_NULL_HANDLE);
        });
    }

    void VKShaderResource::SetStorage(std::string_view param, RefPtr<StorageBuffer> storage)
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
    }

    void VKShaderResource::PrepareResource(VkCommandBuffer cmdBuf)
    {

        for(auto& f : pendingTextureUpdates)
        {
            f();
        }
        pendingTextureUpdates.clear();

        // TODO: the pipeline stage can be simplified by querying the binding usage from shader info

        for(auto& tex : textures)
        {
            tex.second->TransformLayoutIfNeeded(cmdBuf,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_SHADER_READ_BIT
                    );
        }

        for(auto& buf : uniformBuffers)
        {
            buf.second->PutMemoryBarrierIfNeeded(cmdBuf, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }

        for(auto& buf : storageBuffers)
        {
            buf.second->PutMemoryBarrierIfNeeded(cmdBuf, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
    }

    RefPtr<ShaderProgram> VKShaderResource::GetShader()
    {
        return shaderProgram.Get();
    }
}
