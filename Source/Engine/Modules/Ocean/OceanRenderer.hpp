#pragma once
#include "OceanQuadTree.hpp"

#include "Core/Graphics/Mesh.hpp"
#include "Core/Ptr.hpp"
#include "Rendering/GPUBuffer.hpp"
#include "Rendering/RenderingData.hpp"

#include "GfxDriver/Buffer.hpp"
#include "Modules/Ocean/OceanQuadTree.hpp"
#include "Rendering/Shader.hpp"

class OceanRenderer
{
public:
    OceanRenderer();

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
        std::unique_ptr<Gfx::Buffer> buffer = nullptr;
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
    };

    GPUBuffer<RendererInputBuffer> rendererInputUBO;

    ObjPtr<Shader> oceanPatchShader;
    std::unique_ptr<Gfx::ShaderResource> patchRenderShaderResource;

    std::vector<PatchLodData> patchLodDatas{};
    int oceanParamsSetIndex;
    int oceanMaterialSetIndex;

    void EnsureInstanceBufferSize(size_t count);
    void InitPatchLodData(std::span<int> vertexSize);
    void FillInstanceData(OceanQuadTree& quadTree);
    void DrawPatches(Gfx::CommandBuffer& cmd, Material& waveMaterial, const Rendering::RenderingData& data);
};
