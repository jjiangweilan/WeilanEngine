#pragma once
#include "Libs/Ptr.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKContext;
class VKDescriptorPool
{
public:
    VKDescriptorPool(RefPtr<VKContext> context, VkDescriptorSetLayoutCreateInfo& layoutCreateInfo);
    VKDescriptorPool(const VKDescriptorPool& other) = delete;
    VKDescriptorPool(VKDescriptorPool&& other);
    VkDescriptorSetLayout GetLayout()
    {
        return layout;
    }
    VkDescriptorSet Allocate();

    // TODO: we can't directly deallocate or free a set because it's probably still in use
    // maybe we can have vkdriver provide a way to register a callback to be called after each cmd is finished (test by
    // vkFence)
    void Deallocate(VkDescriptorSet set)
    {
        freeSets.push_back(set);
    }

    void Free(VkDescriptorSet set);

    ~VKDescriptorPool();

private:
    VkDescriptorPoolCreateInfo createInfo{};
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorPoolSize> poolSizes;
    RefPtr<VKContext> context;

    std::vector<VkDescriptorPool> fullPools{};
    std::vector<VkDescriptorSet> freeSets;
    VkDescriptorPool freePool = VK_NULL_HANDLE;

    VkDescriptorPool CreateNewPool();
};

struct VkDescriptorSetLayoutCreateInfoHash
{
    std::size_t operator()(const VkDescriptorSetLayoutCreateInfo& c) const;
};

// TODO: is this bad? too many pools are created
struct VKDescriptorPoolCache
{
    VKDescriptorPoolCache(RefPtr<VKContext> context) : context(context) {}
    VKDescriptorPool& RequestDescriptorPool(const std::string& shaderName, VkDescriptorSetLayoutCreateInfo createInfo);

private:
    // we hash manually and use std::size_t as key to avoid dangling pointer of createInfo
    std::unordered_map<VkDescriptorSetLayoutCreateInfo, VKDescriptorPool, VkDescriptorSetLayoutCreateInfoHash>
        descriptorLayoutPoolCache;
    RefPtr<VKContext> context;

private:
};
} // namespace Gfx

inline bool operator==(const VkDescriptorSetLayoutCreateInfo& first, const VkDescriptorSetLayoutCreateInfo& second)
{
    bool isSame = first.sType == second.sType && first.pNext == second.pNext && first.flags == second.flags &&
                  first.bindingCount == second.bindingCount;

    if (isSame)
    {
        for (uint32_t i = 0; i < first.bindingCount; ++i)
        {
            isSame = isSame && first.pBindings[i].binding == second.pBindings[i].binding &&
                     first.pBindings[i].descriptorType == second.pBindings[i].descriptorType &&
                     first.pBindings[i].descriptorCount == second.pBindings[i].descriptorCount &&
                     first.pBindings[i].stageFlags == second.pBindings[i].stageFlags;

            if (isSame && first.pBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
                for (size_t j = 0; j < first.pBindings[i].descriptorCount; j++)
                {
                    isSame = isSame && first.pBindings[i].pImmutableSamplers == second.pBindings[i].pImmutableSamplers;
                }
            }
        }
    }

    return isSame;
}
