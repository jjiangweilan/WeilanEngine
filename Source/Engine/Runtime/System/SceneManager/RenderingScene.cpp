#include "RenderingScene.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/Component/SceneEnvironment.hpp"

SceneEnvironmentData& RenderingScene::GetSceneEnvironmentData()
{
    static SceneEnvironmentData defaultData{
        .fogPassParameters =
            {
                .enabled = false
            }
    };

    if (sceneEnvironment)
    {
        return sceneEnvironment->data;
    }
    return defaultData;
}

Gfx::RayTracingMeshHandle RenderingScene::CreateBLAS(std::span<Gfx::BlasGeometry> geometries)
{
    if (rayTracingContext)
    {
        return rayTracingContext->CreateBLAS(geometries);
    }
    return 0;
}

Gfx::RayTracingInstanceHandle RenderingScene::CreateInstance(MeshRenderer* renderer, Gfx::RayTracingMeshHandle mesh, glm::float4x3 transform)
{
    if (rayTracingContext)
    {
        uint32_t customIndex = static_cast<uint32_t>(rayTracingInstances.size());
        auto handle = rayTracingContext->CreateInstance(mesh, transform, customIndex);
        rayTracingInstances.push_back({handle, renderer});
        needsTLASRebuild = true;
        return handle;
    }
    return 0;
}

void RenderingScene::UpdateRayTracingInstance(Gfx::RayTracingInstanceHandle instance, glm::float4x3 transform)
{
    if (rayTracingContext == nullptr || instance < 0)
        return;

    rayTracingContext->UpdateInstanceTransform(instance, transform);
    needsTLASRebuild = true;
}

void RenderingScene::ResetRuntimeState()
{
    renderingObjects.Clear();
    particleSystems.clear();
    meshRenderers.clear();
    gpuObjectRenderers.clear();
    grassSurfaces.clear();
    clouds.clear();
    sceneEnvironment = nullptr;
    terrain = nullptr;
    rayTracingInstances.clear();
    rtObjectOffsetsBuffer = nullptr;
    needsTLASRebuild = false;
}

void RenderingScene::Tick()
{
    if (rayTracingScene == 0 && rayTracingContext != nullptr)
    {
        rayTracingScene = rayTracingContext->CreateScene(8192);
    }

    for (auto m : meshRenderers)
    {
        m->UpdateSkinning();
    }

    if (rayTracingContext != nullptr && needsTLASRebuild && !rayTracingInstances.empty())
    {
        std::vector<Gfx::RayTracingInstanceHandle> handles;
        std::vector<uint32_t> offsets;
        for (const auto& instance : rayTracingInstances)
        {
            handles.push_back(instance.handle);
            offsets.push_back(instance.renderer->GetGpuObjectDescriptor().dataAlloc.offset);
        }

        rayTracingContext->BuildScene(rayTracingScene, handles);

        size_t bufferSize = offsets.size() * sizeof(uint32_t);
        if (rtObjectOffsetsBuffer == nullptr || rtObjectOffsetsBuffer->GetSize() < bufferSize)
        {
            rtObjectOffsetsBuffer = GetGfxDriver()->CreateBuffer(glm::max((size_t)1024, bufferSize), Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst, false, false, "RTObjectOffsetsBuffer");
        }
        GetGfxDriver()->UploadBuffer(*rtObjectOffsetsBuffer, (uint8_t*)offsets.data(), bufferSize, 0);

        Rendering::GPUDrivenManager::Instance().SetRTObjectOffsetBuffer(rtObjectOffsetsBuffer.get());

        needsTLASRebuild = false;
    }
}
