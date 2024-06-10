#pragma once
#include <vector>
#include <vulkan/vulkan.h>
namespace Gfx
{
class VKInstance
{
public:
    VKInstance(const std::vector<const char*>& requiredExtension);
    ~VKInstance();
    VkInstance GetHandle()
    {
        return vulkanInstance;
    }

private:
    VkInstance vulkanInstance;
    VkDebugUtilsMessengerEXT debugMessenger;

    bool CheckAvalibilityOfValidationLayers(const std::vector<const char*>& validationLayers);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    );
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
    );

    class GfxContext;
};

} // namespace Gfx
