#pragma once
#include <cinttypes>
#include <span>

namespace RenderCoreModule
{
class RenderCore;
typedef uint32_t MeshHandleIndex;
} // namespace RenderCoreModule

class MeshHandle
{
public:
    MeshHandle(RenderCoreModule::RenderCore* renderCoreImpl, RenderCoreModule::MeshHandleIndex handleIndex) : handleIndex(handleIndex), impl(renderCoreImpl) {}
    void UploadMeshData(std::span<uint8_t> vertexData, std::span<uint8_t> indexData);

    RenderCoreModule::MeshHandleIndex GetHandleIndex()
    {
        return handleIndex;
    }

private:
    RenderCoreModule::MeshHandleIndex handleIndex;
    RenderCoreModule::RenderCore* impl;
};
