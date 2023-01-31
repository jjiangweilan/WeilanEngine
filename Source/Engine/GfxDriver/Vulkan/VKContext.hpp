#pragma once
#include "Internal/VKDevice.hpp"
#include "Internal/VKMemAllocator.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKSwapChain.hpp"
#include "Libs/Ptr.hpp"
#include "VKDescriptorPool.hpp"
#include "VKSharedResource.hpp"
namespace Engine::Gfx
{
class VKDevice;
class VKMemAllocator;
class VKObjectManager;
class VKSharedResource;
class VKSwapChain;
class VKDriver;
class VKContext
{
public:
    static inline RefPtr<VKContext> Instance() { return context; }
    RefPtr<VKDevice> device;
    RefPtr<VKMemAllocator> allocator;
    RefPtr<VKObjectManager> objManager;
    RefPtr<VKSharedResource> sharedResource;
    RefPtr<VKSwapChain> swapchain;
    RefPtr<const VKCommandQueue> mainQueue;
    RefPtr<VKDescriptorPoolCache> descriptorPoolCache;

private:
    static RefPtr<VKContext> context;
    friend class VKDriver;
};

inline RefPtr<VKDevice> GetDevice() { return VKContext::Instance()->device; }
inline RefPtr<VKMemAllocator> GetMemAllocator() { return VKContext::Instance()->allocator; }
inline RefPtr<VKObjectManager> GetObjManager() { return VKContext::Instance()->objManager; }
} // namespace Engine::Gfx
