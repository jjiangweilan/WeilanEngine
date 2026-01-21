#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/GfxStruct.hpp"
#include <vk_mem_alloc.h> // for virtual memory allocator

namespace RenderCoreModule
{
struct Mesh
{
    VmaVirtualAllocation vertexHandle = 0;
    uint64_t vertexOffset = 0;

    VmaVirtualAllocation indexHandle = 0;
    uint64_t indexOffset = 0;
};
} // namespace RenderCoreModule
