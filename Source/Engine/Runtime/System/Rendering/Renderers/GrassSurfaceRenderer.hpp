#pragma once
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "GrassSurfaceStructs.hpp"

class GrassSurfaceRenderer
{
public:
    GrassSurfaceRenderer();
    ~GrassSurfaceRenderer();

    void Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData);

private:
    struct GrassPatchBatch
    {
        Mesh* mesh = nullptr;
        uint32_t instanceOffset = 0;
        uint32_t instanceCount = 0;
    };

    struct PushConstant
    {
        uint32_t instanceOffset = 0;
        float3 albedo = {0.25f, 0.65f, 0.18f};
    };

    ObjPtr<Shader> grass;
    PipelineGPUBuffer instanceBuffer;
    std::vector<GrassPatchInstanceData> instances;
    std::vector<GrassPatchBatch> batches;
    int paramsSetIndex = 0;
};
