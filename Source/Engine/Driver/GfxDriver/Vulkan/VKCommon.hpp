#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif

#if defined(_WIN32) || defined(_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// volk includes vulkan_win32 when VK_USE_PLATFORM_WIN32_KHR is defined
#include <volk.h>

// we use volk to load vulkan function, so we don't want VMA to load vulkan functions by itself
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#undef VK_USE_PLATFORM_WIN32_KHR // don't let vulkan.h include Windows.h
#include <vk_mem_alloc.h>
#define VK_USE_PLATFORM_WIN32_KHR
