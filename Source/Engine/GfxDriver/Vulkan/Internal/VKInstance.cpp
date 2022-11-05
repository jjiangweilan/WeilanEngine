#include "VKInstance.hpp"

#include <cstring>
#include <spdlog/spdlog.h>

namespace Engine::Gfx
{
    VKInstance::VKInstance(const std::vector<const char*>& requiredExtension)
    {
        bool enableValidationLayers = true;

        // Create vulkan application info
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Core Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 193);
        appInfo.pNext = VK_NULL_HANDLE;

        VkInstanceCreateInfo createInfo{};

        // Create vulkan instance info
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
        createInfo.pNext = VK_NULL_HANDLE;

        std::vector<const char*> extensions = requiredExtension;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = VkDebugUtilsMessengerCreateInfoEXT{};
        std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"}; // If you don't get syncrhonization validation work, be sure it's enabled and overrided in vkconfig app in VulkanSDK
        if (enableValidationLayers)
        {
            if (!CheckAvalibilityOfValidationLayers(validationLayers))
            {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            // VK_EXT_DEBUG_UTILS_EXTENSION_NAME enables message callback
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // Enable Debug message
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity =
                //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = this->DebugCallback;
            debugCreateInfo.pUserData = nullptr; // Optional

            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }

        // Enable instance extension
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        auto result = vkCreateInstance(&createInfo, nullptr, &this->vulkanInstance);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }

        if (enableValidationLayers)
        {
            // If we enable validation layer, then we also want to enable debug messenger
            if (CreateDebugUtilsMessengerEXT(this->vulkanInstance, &debugCreateInfo, nullptr, &this->debugMessenger) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
    }

    VKInstance::~VKInstance()
    {
        if (debugMessenger != VK_NULL_HANDLE)
        {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vulkanInstance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr)
            {
                func(vulkanInstance, debugMessenger, nullptr);
            }
        }

        vkDestroyInstance(vulkanInstance, nullptr);
    }

    bool VKInstance::CheckAvalibilityOfValidationLayers(const std::vector<const char *> &validationLayers)
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for(auto& k : availableLayers)
        {
            SPDLOG_INFO(k.layerName);
        }
        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    VkBool32 VKInstance::DebugCallback(
     VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
     VkDebugUtilsMessageTypeFlagsEXT messageType, 
     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
     void* pUserData)
     {
         switch(messageSeverity)
         {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                SPDLOG_INFO(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                SPDLOG_INFO(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                SPDLOG_WARN(pCallbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                SPDLOG_ERROR(pCallbackData->pMessage);
                break;
            default: 
                break;
         }

        return VK_FALSE;
    }

    VkResult VKInstance::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
}
