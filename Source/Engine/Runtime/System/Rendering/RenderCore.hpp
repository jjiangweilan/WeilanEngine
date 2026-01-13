#pragma once
#include "RenderCore/RenderCoreData.hpp"
#include <memory>
#include <span>

class Scene;
class Camera;
class RenderCoreImpl;
class RenderCore
{
public:
    static RenderCore& Singleton();

    MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16);
    void DestroyMesh(MeshHandle handle);
    void RenderScene(Scene* scene, std::span<Camera*> camera);

    void UploadMeshData(MeshHandle& handle, std::span<uint8_t> vertexData, std::span<uint8_t> indexData);

private:
    std::unique_ptr<RenderCoreImpl> impl = nullptr;
};
