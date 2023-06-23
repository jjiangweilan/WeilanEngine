#include "VKDevice.hpp"
#include "VKInstance.hpp"
#include "VKPhysicalDevice.hpp"
#include "VKSurface.hpp"

#include <format>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

namespace Engine::Gfx
{
VKDevice::VKDevice(VKInstance* instance, VKSurface* surface, QueueRequest* queueRequests, int requestsCount)
    : gpu(VKPhysicalDevice::SelectGPUAndQueryDataForSurface(*instance, *surface))
{
    assert(requestsCount < 16);
    uint32_t queueFamilyIndices[16];
    float queuePriorities[16][16];
    auto& queueFamilyProperties = gpu.GetQueueFamilyProperties();
    for (int i = 0; i < requestsCount; ++i)
    {
        QueueRequest request = queueRequests[i];
        int queueFamilyIndex = 0;
        bool found = false;
        for (; queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex)
        {
            if (queueFamilyProperties[queueFamilyIndex].queueFlags & request.flags)
            {
                VkBool32 surfaceSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu.GetHandle(),
                                                     queueFamilyIndex,
                                                     surface->GetHandle(),
                                                     &surfaceSupport);
                if (surfaceSupport && request.requireSurfaceSupport)
                {
                    found = true;
                    break;
                }

                if (!request.requireSurfaceSupport)
                {
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            throw std::runtime_error("Vulkan: Can't find required queue family index");

        queueFamilyIndices[i] = queueFamilyIndex;
        queuePriorities[i][0] = request.priority;
    }

    VkDeviceQueueCreateInfo queueCreateInfos[16];

    int queueCreateInfoCount = 0;
    for (int i = 0; i < requestsCount; ++i)
    {
        bool skip = false;
        // found duplicate queueFamilyIndex
        for (int j = 0; j < i; ++j)
        {
            if (queueCreateInfos[j].queueFamilyIndex == queueFamilyIndices[i])
            {
                queueCreateInfos[j].queueCount += 1;
                queuePriorities[j][queueCreateInfos[j].queueCount - 1] = queueRequests[i].priority;
                skip = true;
                break;
            }
        }

        if (!skip)
        {
            queueCreateInfos[i].flags = 0;
            queueCreateInfos[i].pNext = VK_NULL_HANDLE;
            queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfos[i].queueFamilyIndex = queueFamilyIndices[i];
            queueCreateInfos[i].queueCount = 1;
            queueCreateInfos[i].pQueuePriorities = queuePriorities[i];
            queueCreateInfoCount += 1;
        }
    }

    VkDeviceCreateInfo deviceCreateInfo = {};

#if __APPLE__
    for (auto extension : gpu.GetAvailableExtensions())
    {
        if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
        {
            deviceExtensions.push_back("VK_KHR_portability_subset");
        }
    }
#endif

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = VK_NULL_HANDLE;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;

    deviceCreateInfo.pEnabledFeatures = &requiredDeviceFeatures;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = VK_NULL_HANDLE;

    vkCreateDevice(gpu.GetHandle(), &deviceCreateInfo, VK_NULL_HANDLE, &deviceHandle);

    // Get the device' queue
    for (int i = 0; i < requestsCount; ++i)
    {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueIndex = 0;
        // make sure each queue is unique
        for (int j = 0; j < queues.size(); ++j)
        {
            if (queues[j].queueIndex == queueIndex)
            {
                queueIndex += 1;
            }
        }
        vkGetDeviceQueue(deviceHandle, queueFamilyIndices[i], queueIndex, &queue);

        assert(queue != VK_NULL_HANDLE);
        VKCommandQueue q;
        q.queue = queue;
        q.queueIndex = queueIndex;
        q.queueFamilyIndex = queueFamilyIndices[i];
        queues.push_back(q);
    }
}

VKDevice::~VKDevice() { vkDestroyDevice(deviceHandle, nullptr); }

void VKDevice::WaitForDeviceIdle()
{
    VkResult result = vkDeviceWaitIdle(deviceHandle);
    if (result != VK_SUCCESS)
    {
        SPDLOG_ERROR("device failed to wait");
    }
}
} // namespace Engine::Gfx
