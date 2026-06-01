#pragma once
#include "Engine/Runtime/Object/Component/GrassSurface.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/ImageView.hpp"
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
        const Rendering::RenderingData& renderingData
    );
    void DrawMotionVectors(
        GrassSurface& grassSurface,
        Gfx::CommandBuffer& cmd,
        const Rendering::RenderingData& renderingData
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

    struct GrassParam
    {
        glm::float4 grassColorRamp_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
        glm::float4 grassColorRamp_Top = {0.6f, 0.9f, 0.2f, 1.0f};
        glm::float4 grassColorRamp2_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
        glm::float4 grassColorRamp2_Top = {0.6f, 0.9f, 0.2f, 1.0f};
        glm::float4 grassColorRamp3_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
        glm::float4 grassColorRamp3_Top = {0.6f, 0.9f, 0.2f, 1.0f};
        glm::float4 grassMaskUVScaler = {1.0f, 1.0f, 1.0f, 1.0f};
        float hueShift_0 = 0.0f;
        float hueShift_1 = 0.0f;
        float windScale = 1.0f;
    };

    ObjPtr<Shader> grass;
    ObjPtr<Shader> grassMotionVector;
    PipelineGPUBuffer instanceBuffer;
    PipelineGPUBuffer grassParamBuffer;
    std::vector<GrassPatchInstanceData> instances;
    std::vector<GrassPatchBatch> batches;
    int paramsSetIndex = 0;
    int motionVectorParamsSetIndex = 0;

    bool PrepareDrawData(GrassSurface& grassSurface, const Rendering::RenderingData& renderingData);
    void BindGrassParams(GrassPatchGroup& group, Gfx::CommandBuffer& cmd, int setIndex, const Rendering::RenderingData& renderingData);
};
