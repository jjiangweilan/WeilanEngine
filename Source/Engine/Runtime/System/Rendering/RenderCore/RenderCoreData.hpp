#pragma once

#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/GfxStruct.hpp"
#include <vk_mem_alloc.h> // for virtual memory allocator

class MeshHandle
{
public:
    void UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData);

private:
    VmaVirtualAllocation vertexHandle = 0;
    uint64_t vertexOffset = 0;

    VmaVirtualAllocation indexHandle = 0;
    uint64_t indexOffset = 0;

    friend class RenderCoreImpl;
    friend class UploadMeshDataCmd;
};
