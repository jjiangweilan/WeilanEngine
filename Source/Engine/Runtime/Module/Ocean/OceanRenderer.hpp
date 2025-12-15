#pragma once
#include "OceanQuadTree.hpp"

#include "Runtime/Object/Graphics/Mesh.hpp"
#include "Core/Ptr.hpp"
#include "Runtime/System/Rendering/GPUBuffer.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"

#include "Driver/GfxDriver/Buffer.hpp"
#include "Runtime/Module/Ocean/OceanQuadTree.hpp"
#include "Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Runtime/System/Rendering/Shader.hpp"

class OceanRenderer
{
public:
    OceanRenderer();
    ~OceanRenderer();

    // vertexSize's count should match quadTree's LOD level count
    void Setup(std::span<int> vertexSize);
    void Render(Gfx::CommandBuffer& cmd, OceanQuadTree& quadTree, Material& waveMaterial, const Rendering::RenderingData& renderingData);

private:
    // MAKE SURE this is stricly packed
    struct GPUNodeInstanceData
    {
        float2 position;
        float2 scale;
    };

    struct
    {
        PipelineGPUBuffer buffer;
        std::vector<GPUNodeInstanceData> cpuData;
        int count = 0;
    } instanceBuffer;

    struct PatchDesc
    {
        static const int meter = 1; // not really meaningful parameter, the mesh will be scaled eventually to match the node size, change vertices to adjust resolution
        int vertices = 513;
    };

    struct PatchLodData
    {
        PatchDesc desc;
        std::unique_ptr<Mesh> patch;
        int instanceDataOffset;
        int instanceCount;
    };

    struct RendererInputBuffer
    {
        float4 depthTexSize;
    } rendererInputBuffer = {};
    PipelineGPUBuffer rendererInputUBO;

    ObjPtr<Shader> oceanPatchShader;

    std::vector<PatchLodData> patchLodDatas{};
    int oceanParamsSetIndex;
    int oceanMaterialSetIndex;

    void EnsureInstanceBufferSize(size_t count, const Rendering::RenderingData&);
    void InitPatchLodData(std::span<int> vertexSize);
    void FillInstanceData(OceanQuadTree& quadTree);
    void DrawPatches(Gfx::CommandBuffer& cmd, Material& waveMaterial, const Rendering::RenderingData& data);
};
