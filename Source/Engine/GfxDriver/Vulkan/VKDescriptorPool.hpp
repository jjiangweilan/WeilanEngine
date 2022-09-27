#pragma once
#include "Code/Ptr.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
namespace Engine::Gfx
{
    struct VKContext;
    class VKDescriptorPool
    {
        public:
            VKDescriptorPool(RefPtr<VKContext> context, VkDescriptorSetLayoutCreateInfo& layoutCreateInfo);
            VKDescriptorPool(const VKDescriptorPool& other) = delete;
            VKDescriptorPool(VKDescriptorPool&& other);
            VkDescriptorSetLayout GetLayout() {return layout;}
            VkDescriptorSet Allocate();
            
            void Free(VkDescriptorSet set)
            {
                freeSets.push_back(set);
            }

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

    struct VKDescriptorPoolCache
    {
        static RefPtr<VKDescriptorPoolCache> Instance() {return singleton;}

        static void Init(RefPtr<VKContext> c)
        {
            if (singleton == nullptr)
            {
                singleton = UniPtr<VKDescriptorPoolCache>( new VKDescriptorPoolCache(c));
            }
        }

        static void Deinit()
        {
            singleton->descriptorLayoutPoolCache.clear();
            singleton = nullptr;
        }

        VKDescriptorPool& RequestDescriptorPool(VkDescriptorSetLayoutCreateInfo createInfo);

        private:
            VKDescriptorPoolCache(RefPtr<VKContext> context) : context(context){}
            struct VkDescriptorSetLayoutCreateInfoHash
            {
                std::size_t operator()(const VkDescriptorSetLayoutCreateInfo& c) const;
            };

            std::unordered_map<VkDescriptorSetLayoutCreateInfo, VKDescriptorPool, VkDescriptorSetLayoutCreateInfoHash> descriptorLayoutPoolCache;
            RefPtr<VKContext> context;

        private:
            static UniPtr<VKDescriptorPoolCache> singleton;
            friend UniPtr<VKDescriptorPoolCache> MakeUnique<VKDescriptorPoolCache, RefPtr<VKContext>&>(RefPtr<VKContext>& context);
    };
}

inline bool operator==(const VkDescriptorSetLayoutCreateInfo& first, const VkDescriptorSetLayoutCreateInfo& second)
{
    bool isSame = first.sType == second.sType && 
    first.pNext == second.pNext &&
    first.flags == second.flags &&
    first.bindingCount == second.bindingCount;

    for(uint32_t i = 0; i < first.bindingCount; ++i)
    {
        isSame = isSame &&
        first.pBindings[i].binding == second.pBindings[i].binding &&
        first.pBindings[i].descriptorType == second.pBindings[i].descriptorType &&
        first.pBindings[i].descriptorCount == second.pBindings[i].descriptorCount &&
        first.pBindings[i].stageFlags == second.pBindings[i].stageFlags &&
        first.pBindings[i].pImmutableSamplers == second.pBindings[i].pImmutableSamplers;
    }

    return isSame;
}