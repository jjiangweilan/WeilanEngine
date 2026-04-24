#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/GPUBuffer.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

namespace GPUResources::ReflectionProbe
{
#include "Engine/Shaders/ReflectionProbeIBLGeneratorInput.hlsl"
};

class ReflectionProbe;

namespace Rendering::Passes
{
class ReflectionProbeUpdate : public RenderPipelinePass
{
    ObjPtr<Shader> iblGenerator;
    std::unordered_map<UUID, std::unique_ptr<Gfx::ShaderResource>> probeShaderResources;
    std::unique_ptr<Gfx::Image> cubemap; // final cubemap
    std::unique_ptr<Gfx::Image> cubemapBase;
    std::vector<std::unique_ptr<Gfx::ImageView>> cubemapImageViews;
    Gfx::DescriptorSetSemantics shaderInputSet;

    std::unique_ptr<Gfx::Buffer> spdGlobalAtomic;
    std::unique_ptr<Gfx::Image> rw_input_downsample_src_mid_mip;
    std::unique_ptr<Gfx::ShaderResource> spdInput;
    ObjPtr<Shader> spdShader;
    std::unique_ptr<Gfx::ShaderResource> probeUpdateShaderResource;
    float roughness[6];
    Material cubemapBaseMat;
    int totalPixelCount;

    GPUBuffer<GPUResources::ReflectionProbe::ParameterInput> shaderInput = GPUBuffer<GPUResources::ReflectionProbe::ParameterInput>(true);

public:
    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData);
    Gfx::Image* GetIBLCubemap() { return cubemap.get(); }

    void OnInit(RenderingData* renderingData) override;

private:
    Gfx::ShaderResource* EnsureProbeShaderResource(ReflectionProbe& probe);
    void MipmapGeneration(Gfx::CommandBuffer& cmd, uint32_t width, uint32_t height, Gfx::Image& src);
    void DrawSkyboxOnProbe(Gfx::CommandBuffer& cmd, Gfx::Image& probe, Material& baseMat, Gfx::ShaderResource& globalSet);
    Gfx::ShaderResource* EnsureAndGetShaderResource(std::vector<std::unique_ptr<Gfx::ImageView>>& cubemapImageViews);

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
