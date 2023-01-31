#pragma once

#include <vector>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
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

    const VkSurfaceKHR& GetHandle() const { return surface; }

    void QuerySurfaceDataFromGPU(VKPhysicalDevice* gpu);

    inline const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() { return surfaceCapabilities; }

    inline const std::vector<VkPresentModeKHR>& GetSurfacePresentModes() { return surfacePresentModes; }

    inline const std::vector<VkSurfaceFormatKHR>& GetSurfaceFormats() { return surfaceFormats; }

private:
    VKInstance& attachedInstance;
    VKDevice* attachedDevice;
    VkSurfaceKHR surface;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkPresentModeKHR> surfacePresentModes;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    friend class GfxContext;
};

} // namespace Engine::Gfx
