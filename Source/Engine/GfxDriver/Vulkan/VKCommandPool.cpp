#include "VKCommandPool.hpp"
#include "VKCommandQueue.hpp"
#include "VKContext.hpp"

namespace Engine::Gfx
{
VKCommandPool::VKCommandPool(const CreateInfo& gCreateInfo)
{
    VkCommandPoolCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.queueFamilyIndex = gCreateInfo.queueFamilyIndex;
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    vkCreateCommandPool(GetDevice()->GetHandle(), &createInfo, VK_NULL_HANDLE, &commandPool);
}

VKCommandPool::~VKCommandPool() { vkDestroyCommandPool(GetDevice()->GetHandle(), commandPool, VK_NULL_HANDLE); }

std::vector<UniPtr<CommandBuffer>> VKCommandPool::AllocateCommandBuffers(CommandBufferType type, int count)
{
    VkCommandBuffer* cmdBufsTemp = new VkCommandBuffer[count];

    VkCommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = VK_NULL_HANDLE;
    cmdAllocInfo.commandPool = commandPool;
    cmdAllocInfo.level =
        type == CommandBufferType::Primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    cmdAllocInfo.commandBufferCount = count;
    vkAllocateCommandBuffers(GetDevice()->GetHandle(), &cmdAllocInfo, cmdBufsTemp);

    std::vector<UniPtr<CommandBuffer>> rlt;
    for (int i = 0; i < count; ++i)
    {
        rlt.push_back(MakeUnique<VKCommandBuffer>(cmdBufsTemp[i]));
    }

    delete[] cmdBufsTemp;
    return rlt;
}

void VKCommandPool::ResetCommandPool()
{
    vkResetCommandPool(GetDevice()->GetHandle(), commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}
} // namespace Engine::Gfx