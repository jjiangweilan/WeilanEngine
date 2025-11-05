#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/GPUBuffer.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderingData.hpp"

namespace GPUResources::ReflectionProbe
{
#include "Shaders/ReflectionProbeIBLGeneratorInput.hlsl"
};

class ReflectionProbe;

namespace Rendering::Passes
{
class ReflectionProbeUpdate
{
    GPUBuffer<GPUResources::ReflectionProbe::ParameterInput> shaderInput = GPUBuffer<GPUResources::ReflectionProbe::ParameterInput>(true);
    ObjPtr<Shader> iblGenerator;
    std::unordered_map<UUID, std::unique_ptr<Gfx::ShaderResource>> probeShaderResources;
    std::unique_ptr<Gfx::Image> cubemap; // final cubemap
    std::vector<std::unique_ptr<Gfx::ImageView>> cubemapImageViews;
    Gfx::DescriptorSetSemantics shaderInputSet;

    std::unique_ptr<Gfx::Buffer> spdGlobalAtomic;
    std::unique_ptr<Gfx::Image> rw_input_downsample_src_mid_mip;
    std::unique_ptr<Gfx::ShaderResource> spdInput;
    ObjPtr<Shader> spdShader;

public:
    ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer);
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe);

private:
    Gfx::ShaderResource* EnsureProbeShaderResource(ReflectionProbe& probe);
    void MipmapGeneration(Gfx::CommandBuffer& cmd, uint32_t width, uint32_t height, Gfx::Image& src);
    void DrawSkyboxOnProbe(Gfx::CommandBuffer& cmd, Gfx::Image& probe, Material& baseMat, Gfx::ShaderResource& globalSet);

    struct ffx_spd_resources
    {
        uint32_t mips;
        uint32_t numWorkGroups;
        uint2 workGroupOffset;
        float2 invInputSize; // Only used for linear sampling mode
        uint2 padding;
    };
    GPUBuffer<ffx_spd_resources> spdBuffer;
};
} // namespace Rendering::Passes
