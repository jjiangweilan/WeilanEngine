#include "RenderCore.hpp"

namespace RenderCoreModule
{
void RenderCore::UploadMeshData(MeshHandleIndex& handleIndex, std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
{
    meshManager.UploadMeshData(handleIndex, vertexData, indexData);
}
} // namespace RenderCoreModule
