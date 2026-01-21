#include "RenderCoreData.hpp"
#include "Engine/Runtime/System/Rendering/RenderCore/RenderCore.hpp"

void MeshHandle::UploadMeshData(std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
{
    impl->UploadMeshData(handleIndex, vertexData, indexData);
}
