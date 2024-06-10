#include "VKSurface.hpp"
#include "VKAppWindow.hpp"
#include "VKInstance.hpp"
#include "VKPhysicalDevice.hpp"
#include <spdlog/spdlog.h>

namespace Gfx
{
VKSurface::VKSurface(VKInstance& instance, VKAppWindow* appWindow) : attachedInstance(instance)
{
    appWindow->CreateVkSurface(instance.GetHandle(), &surface);
}

VKSurface::~VKSurface()
{
    vkDestroySurfaceKHR(attachedInstance.GetHandle(), surface, VK_NULL_HANDLE);
}

void VKSurface::QuerySurfaceDataFromGPU(VKPhysicalDevice* gpu)
{
    auto gpuDevice = gpu->GetHandle();

    // Get surface capabilities
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpuDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
    {
        throw std::runtime_error("Could not check presentation surface capabilities!");
    }

    // Get surface present mode
    uint32_t presentModesCount;
    if ((vkGetPhysicalDeviceSurfacePresentModesKHR(gpuDevice, surface, &presentModesCount, nullptr) != VK_SUCCESS) ||
        (presentModesCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface present modes enumeration!");
    }

    surfacePresentModes.resize(presentModesCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(gpuDevice, surface, &presentModesCount, &surfacePresentModes[0]) !=
        VK_SUCCESS)
    {
        SPDLOG_ERROR("Error occurred during presentation surface present modes enumeration!");
    }

    // Get surface formats
    uint32_t formatsCount;
    if ((vkGetPhysicalDeviceSurfaceFormatsKHR(gpuDevice, surface, &formatsCount, nullptr) != VK_SUCCESS) ||
        (formatsCount == 0))
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }

    surfaceFormats.resize(formatsCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(gpuDevice, surface, &formatsCount, &surfaceFormats[0]) != VK_SUCCESS)
    {
        throw std::runtime_error("Error occurred during presentation surface formats enumeration!");
    }
}
} // namespace Gfx
