#pragma once

#include "Libs/DynamicArray.hpp"
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKPhysicalDevice;
class VKInstance;
class VKDevice;
class VKAppWindow;
class VKSurface
{
public:
    VKSurface(VKInstance& instance, VKAppWindow* appWindow);
    ~VKSurface();

    const VkSurfaceKHR& GetHandle() const
    {
        return surface;
    }

    void QuerySurfaceDataFromGPU(VKPhysicalDevice* gpu);

    inline const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities()
    {
        return surfaceCapabilities;
    }

    inline const DynamicArray<VkPresentModeKHR>& GetSurfacePresentModes()
    {
        return surfacePresentModes;
    }

    inline const DynamicArray<VkSurfaceFormatKHR>& GetSurfaceFormats()
    {
        return surfaceFormats;
    }

private:
    VKInstance& attachedInstance;
    VKDevice* attachedDevice;
    VkSurfaceKHR surface;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    DynamicArray<VkPresentModeKHR> surfacePresentModes;
    DynamicArray<VkSurfaceFormatKHR> surfaceFormats;

    friend class GfxContext;
};

} // namespace Gfx
