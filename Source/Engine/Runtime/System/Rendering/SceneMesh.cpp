#include "SceneMesh.hpp"

void SceneMeshManager::Init(uint32_t totalVertexBufferSize, uint32_t totalIndexBufferSize)
{
    VmaVirtualBlockCreateInfo blockCreateInfo = {};
    blockCreateInfo.size = 1024 * 1024 * 1; // 1 MB

    VkResult res = vmaCreateVirtualBlock(&blockCreateInfo, &virtualVertexBlock);
    ASSERT(res == VK_SUCCESS);
}

bool SceneMeshManager::AllocateMesh(size_t size, SceneMeshHandle& outHandle)
{
    VmaVirtualAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.size = size;

    VkDeviceSize offset;
    VkResult res = VK_ERROR_UNKNOWN;
    res = vmaVirtualAllocate(virtualVertexBlock, &allocCreateInfo, &outHandle._vertexAlloc, &outHandle.vertexBufferOffset);
    res = vmaVirtualAllocate(virtualIndexBlock, &allocCreateInfo, &outHandle._indexAlloc, &outHandle.indexBufferOffset);

    return res == VK_SUCCESS;
}

void SceneMeshManager::FreeMesh(SceneMeshHandle& handle)
{
    vmaVirtualFree(virtualVertexBlock, handle._vertexAlloc);
    vmaVirtualFree(virtualVertexBlock, handle._indexAlloc);

    handle._vertexAlloc = nullptr;
    handle._indexAlloc = nullptr;
}
