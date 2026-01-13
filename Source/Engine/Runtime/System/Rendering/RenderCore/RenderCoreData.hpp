#pragma
#include <vk_mem_alloc.h> // for virtual memory allocator

struct MeshHandle
{
    VmaVirtualAllocation vertexHandle = 0;
    uint64_t vertexOffset = 0;

    VmaVirtualAllocation indexHandle = 0;
    uint64_t indexOffset = 0;
};
