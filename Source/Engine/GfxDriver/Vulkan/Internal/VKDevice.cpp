#include "VKDevice.hpp"
#include "VKInstance.hpp"
#include "VKSurface.hpp"
#include "VKPhysicalDevice.hpp"

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
    VKDevice::VKDevice(VKInstance* instance, VKSurface* surface) : 
        gpu(VKPhysicalDevice::SelectGPUAndQueryDataForSurface(*instance, *surface))
    {
        auto requiredQueueFamilyIndex = std::vector<uint32_t>{gpu.GetGraphicsQueueFamilyIndex()};
        float queuePriorities[] = {1, 0.9};
        auto queueCreateInfos = std::vector<VkDeviceQueueCreateInfo>();

        // TODO, Jiehong Jiang: This is probably a problen what if the two family index are the same?. 2021-11-18
        for (int i = 0; i < requiredQueueFamilyIndex.size(); ++i)
        {
            uint32_t queueFamilyIndex = requiredQueueFamilyIndex[i];
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.flags = 0;
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = queuePriorities;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkDeviceCreateInfo deviceCreateInfo = {};

        for(auto extension : gpu.GetAvailableExtensions())
        {
            if (std::strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
            {
                deviceExtensions.push_back("VK_KHR_portability_subset");
            }
        }

        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = VK_NULL_HANDLE;
        deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

        deviceCreateInfo.pEnabledFeatures = &requiredDeviceFeatures;
        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = VK_NULL_HANDLE;

        vkCreateDevice(gpu.GetHandle(), &deviceCreateInfo, VK_NULL_HANDLE, &deviceHandle);

        // Get the device' queue
        vkGetDeviceQueue(deviceHandle, gpu.GetGraphicsQueueFamilyIndex(), 0, &graphicsQueue);
    }

    VKDevice::~VKDevice()
    {
        vkDestroyDevice(deviceHandle, nullptr);
    }

    void VKDevice::WaitForDeviceIdle()
    {
        VkResult result = vkDeviceWaitIdle(deviceHandle);
        if (result != VK_SUCCESS)
        {
            SPDLOG_ERROR("device failed to wait");
        }
    }
}