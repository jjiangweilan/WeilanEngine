#pragma once
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Library/CommandStream.hpp"
#include "Engine/Library/ObjectPool.hpp"
#include "Engine/Library/SpinLock.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline1/RenderPipeline1.hpp"
#include "Engine/Runtime/System/Rendering/RenderScene.hpp"

#include "Mesh.hpp"
#include "MeshManager.hpp"
#include "RenderCoreData.hpp"
#include <span>

class Scene;
class Camera;

namespace RenderCoreModule
{
class RenderCore;

using RenderCoreCommandStream = CommandStream;

class RenderCore;

class RC_CMC : public CommandStreamContext
{
public:
    RenderCore* rc;
};

struct UploadMeshDataCmd
{
    MeshHandle handle;
    uint8_t* vertexData;
    uint8_t* indexData;
    uint32_t vertexDataSize;
    uint32_t indexDataSize;

    static void Execute(CommandStreamContext* context, void* ptr);
};

class RenderCore
{
public:
    MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16)
    {
        return MeshHandle(
            this,
            meshManager.CreateMesh(vertexByteSize, indexCount, indexBit_16)
        );
    }

    void DestroyMesh(MeshHandle handle)
    {
        meshManager.DestroyMesh(handle.GetHandleIndex());
    }

    void UploadMeshData(MeshHandle& handleIndex, std::span<uint8_t> vertexData, std::span<uint8_t> indexData);

    void FlushCommands();

    void Render(RenderScene& scene, std::span<Camera*> cameras, RenderPipeline1 pipelineHandle);

    Gfx::Buffer* GetMeshBuffer()
    {
        return meshManager.GetMeshBuffer();
    }

    MeshManager& GetMeshManager() { return meshManager; }

    RenderCoreCommandStream cm;
    MeshManager meshManager;
    std::vector<std::unique_ptr<RenderPipeline1>> renderPipelines;
};

} // namespace RenderCoreModule
