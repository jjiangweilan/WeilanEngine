#include "VKDescriptorPool.hpp"
#include "Internal/VKObjectManager.hpp"
#include "VKContext.hpp"
#include <spdlog/spdlog.h>
namespace Engine::Gfx
{
VKDescriptorPool::VKDescriptorPool(RefPtr<VKContext> context, VkDescriptorSetLayoutCreateInfo& layoutCreateInfo)
    : context(context)
{
    std::unordered_map<VkDescriptorType, uint32_t> poolSizesMap;
    for (uint32_t i = 0; i < layoutCreateInfo.bindingCount; ++i)
    {
        auto& b = layoutCreateInfo.pBindings[i];
        poolSizesMap[b.descriptorType] += b.descriptorCount;
    }

    for (auto& iter : poolSizesMap)
    {
        poolSizes.push_back({iter.first, iter.second});
    }

    context->objManager->CreateDescriptorSetLayout(layoutCreateInfo, layout);
}

VKDescriptorPool::~VKDescriptorPool()
{
    if (layout != VK_NULL_HANDLE)
        context->objManager->DestroyDescriptorSetLayout(layout);

    for (auto pool : fullPools)
    {
        context->objManager->DestroyDescriptorPool(pool);
    }

    if (freePool != VK_NULL_HANDLE)
    {
        context->objManager->DestroyDescriptorPool(freePool);
    }
}

VKDescriptorPool::VKDescriptorPool(VKDescriptorPool&& other)
    : createInfo(other.createInfo), layout(std::exchange(other.layout, VK_NULL_HANDLE)),
      poolSizes(std::exchange(other.poolSizes, {})), context(other.context),
      fullPools(std::exchange(other.fullPools, {})), freeSets(std::exchange(other.freeSets, {})),
      freePool(std::exchange(other.freePool, VK_NULL_HANDLE))
{
    createInfo.poolSizeCount = this->poolSizes.size();
    createInfo.pPoolSizes = this->poolSizes.data();
}

VkDescriptorSet VKDescriptorPool::Allocate()
{
    if (!freeSets.empty())
    {
        auto set = freeSets.back();
        freeSets.pop_back();
        return set;
    }

    if (freePool == VK_NULL_HANDLE)
        freePool = CreateNewPool();

    VkDescriptorSetAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.pNext = VK_NULL_HANDLE;
    allocateInfo.descriptorPool = freePool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;
    VkDescriptorSet set;
    VkResult result = vkAllocateDescriptorSets(context->objManager->GetDevice(), &allocateInfo, &set);
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
    {
        // create a new pool and allocate again
        if (freePool != VK_NULL_HANDLE)
            fullPools.push_back(freePool);
        freePool = CreateNewPool();
        allocateInfo.descriptorPool = freePool;
        VkResult result = vkAllocateDescriptorSets(context->objManager->GetDevice(), &allocateInfo, &set);

        if (result != VK_SUCCESS)
        {
            SPDLOG_ERROR("Descriptor pool allocation failed");
            assert(0);
        }
    }

    return set;
}

VkDescriptorPool VKDescriptorPool::CreateNewPool()
{
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.maxSets = createInfo.maxSets == 0 ? 2 : createInfo.maxSets * 2;
    for (auto& poolSize : poolSizes)
    {
        poolSize.descriptorCount *= createInfo.maxSets;
    }
    createInfo.poolSizeCount = this->poolSizes.size();
    createInfo.pPoolSizes = this->poolSizes.data();

    // this can happen when we have an empty descriptor set with zero binding
    VkDescriptorPoolSize dummyPoolSize;
    dummyPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dummyPoolSize.descriptorCount = 1;
    if (createInfo.poolSizeCount == 0)
    {
        createInfo.poolSizeCount = 1;
        createInfo.pPoolSizes = &dummyPoolSize;
    }

    VkDescriptorPool newPool;
    context->objManager->CreateDescriptorPool(createInfo, newPool);
    return newPool;
}

VKDescriptorPool& VKDescriptorPoolCache::RequestDescriptorPool(const std::string& shaderName,
                                                               VkDescriptorSetLayoutCreateInfo createInfo)
{
    std::size_t h = VkDescriptorSetLayoutCreateInfoHash()(createInfo);
    auto it = descriptorLayoutPoolCache.find(h);
    if (it != descriptorLayoutPoolCache.end())
    {
        return it->second;
    }
    else
    {
        auto pair = descriptorLayoutPoolCache.emplace(std::make_pair(h, VKDescriptorPool(context, createInfo)));
        return pair.first->second;
    }
}

std::size_t VKDescriptorPoolCache::VkDescriptorSetLayoutCreateInfoHash::operator()(
    const VkDescriptorSetLayoutCreateInfo& c) const
{
    using std::size_t;
    size_t rlt = c.bindingCount | c.flags << 15;
    for (uint32_t i = 0; i < c.bindingCount; i++)
    {
        auto& b = c.pBindings[i];
        size_t bHash = b.stageFlags << 30 | b.descriptorCount << 24 | b.descriptorType << 16 |
                       reinterpret_cast<std::uintptr_t>(b.pImmutableSamplers) << 8 | b.binding;

        rlt ^= bHash;
    }

    return rlt;
}
} // namespace Engine::Gfx
