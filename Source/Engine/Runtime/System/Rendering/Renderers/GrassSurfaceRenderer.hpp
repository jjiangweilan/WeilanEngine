#pragma once
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/ImageView.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "GrassSurfaceStructs.hpp"

class GrassSurfaceRenderer
{
public:
    GrassSurfaceRenderer();
    ~GrassSurfaceRenderer();

    void Draw(
        GrassSurface& grassSurface,
        Gfx::CommandBuffer& cmd,
        const Rendering::RenderingData& renderingData,
        Gfx::ImageView* shadowMap,
        const Gfx::ImageIdentifier& contactShadowMap,
        Gfx::ImageView* pointLightShadowMap,
        const GPUParameter::DeferredPBRShadingInput& lightingInput
    );

private:
    struct GrassPatchBatch
    {
        Mesh* mesh = nullptr;
        uint32_t instanceOffset = 0;
        uint32_t instanceCount = 0;
    };

    struct PushConstant
    {
        float4 albedo = {0.25f, 0.65f, 0.18f, 1.0f};
        uint32_t instanceOffset = 0;
        float scale = 1.0f;
        uint32_t padding[2] = {};
    };

    ObjPtr<Shader> grass;
    PipelineGPUBuffer instanceBuffer;
    PipelineGPUBuffer lightingInputBuffer;
    std::vector<GrassPatchInstanceData> instances;
    std::vector<GrassPatchBatch> batches;
    int paramsSetIndex = 0;
};
