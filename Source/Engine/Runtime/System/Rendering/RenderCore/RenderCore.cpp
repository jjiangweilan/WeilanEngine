#include "RenderCore.hpp"

namespace RenderCoreModule
{
void UploadMeshDataCmd::Execute(CommandStreamContext* context, void* ptr)
{
    RC_CMC* rcContext = static_cast<RC_CMC*>(context);
    RenderCore* rc = rcContext->rc;
    MeshManager& meshManager = rc->GetMeshManager();
    UploadMeshDataCmd* cmd = (UploadMeshDataCmd*)ptr;
    Gfx::Buffer* meshBuffer = rc->GetMeshBuffer();

    MeshHandle& handle = cmd->handle;
    Mesh* mesh = meshManager.GetMesh(handle.GetHandleIndex());

    if (mesh)
    {
        GetGfxDriver()->UploadBuffer(*meshBuffer, cmd->indexData, cmd->indexDataSize, mesh->indexOffset);
        GetGfxDriver()->UploadBuffer(*meshBuffer, cmd->vertexData, cmd->vertexDataSize, mesh->vertexOffset);
    }
}

void RenderCore::UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData)
{
    void* rawData = nullptr;

    UploadMeshDataCmd* cmd = cm.Push<UploadMeshDataCmd>(
        &UploadMeshDataCmd::Execute,
        &rawData,
        vertexData.size() + indexData.size()
    );

    cmd->handle = handle;
    cmd->vertexData = (uint8_t*)rawData;
    cmd->vertexDataSize = static_cast<uint32_t>(vertexData.size());
    memcpy(cmd->vertexData, vertexData.data(), vertexData.size());

    cmd->indexData = cmd->vertexData + vertexData.size();
    cmd->indexDataSize = static_cast<uint32_t>(indexData.size());
    memcpy(cmd->indexData, indexData.data(), indexData.size());
}
} // namespace RenderCoreModule
