#pragma once
#include "OceanQuadTree.hpp"

#include "Core/Graphics/Mesh.hpp"
#include "Core/Ptr.hpp"
#include "Rendering/RenderingData.hpp"

#include "GfxDriver/Buffer.hpp"
#include "Modules/Ocean/OceanQuadTree.hpp"
#include "Rendering/Shader.hpp"

class OceanRenderer
{
public:
    OceanRenderer();

    void Setup();
    void Render(Gfx::CommandBuffer& cmd, OceanQuadTree& quadTree, const Rendering::RenderingData& renderingData);

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

    std::unique_ptr<Mesh> patch;
    ObjPtr<Shader> oceanPatchShader;
    std::unique_ptr<Gfx::ShaderResource> patchRenderShaderResource;

    struct
    {
        int meter = 32;
        int vertices = 33;
    } meshDesc;

    void EnsureInstanceBufferSize(size_t count);
    void FillInstanceData(OceanQuadTree& quadTree);
    void DrawPatches(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& data);
};
