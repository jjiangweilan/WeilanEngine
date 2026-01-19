#pragma once
#include "Engine/Runtime/System/Rendering/RenderCamera.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline1/RenderPipeline1.hpp"
#include "Engine/Runtime/System/Rendering/RenderScene.hpp"
#include "RenderCore/RenderCoreData.hpp"
#include <memory>
#include <span>

class Scene;
class RenderCoreImpl;

struct RenderCoreInitializeInfo
{
    bool enableBrixelGI = true;
};

class RenderCore
{
public:
    static RenderCore& Singleton();

    void Initialize(RenderCoreInitializeInfo initInfo);

    MeshHandle CreateMesh(size_t vertexByteSize, size_t indexCount, bool indexBit_16);
    void DestroyMesh(MeshHandle handle);

    RenderPipeline1 CreateSceneRenderPipeline();
    RenderScene CreateRenderScene();
    void Render(RenderScene& scene, std::span<RenderCamera*> cameras, RenderPipeline1 pipelineHandle);

private:
    std::unique_ptr<RenderCoreImpl> impl = nullptr;
};
