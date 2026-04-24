#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#include <volk.h>

// we use volk to load vulkan function, so we don't want VMA to load vulkan functions by itself
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#undef VK_USE_PLATFORM_WIN32_KHR // because vk_mem_alloc.h include vulkan.h we don't want it include Windows.h
#include <vk_mem_alloc.h>
#define VK_USE_PLATFORM_WIN32_KHR
