#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/GPUParameter.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Rendering/RenderingData.hpp"
#include "Core/Texture.hpp"

namespace Rendering::Passes
{
class ShadingPass : public RenderPipelinePass
{
public:
    ShadingPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& albedoGBuffer,
        const Gfx::ImageIdentifier& normalGBuffer,
        const Gfx::ImageIdentifier& maskGBuffer,
        Gfx::ImageView* depthImageView,
        Gfx::ImageView* shadowMap,
        Gfx::ImageIdentifier* ssaoTex,
        Gfx::ImageIdentifier* contactShadowMap,
        Gfx::ImageView* diffuseCube,
        Gfx::ImageView* specularCube,
        RenderingData& renderingData
    );

    void UploadGPUParameter(
        float4 shadowMapTexelSize,
        float shadowConstantBias,
        float shadowNormalBias
    );

    void OnInit(RenderingData* renderingData) override;

private:
    GPUParameter::DeferredPBRShadingInput cpuParameter{};
    std::unique_ptr<Gfx::ShaderResource> gpuResource;
    std::unique_ptr<Gfx::Buffer> perMaterialBuffer;
    ObjPtr<Shader> shadingShader;
    Texture* brdfPreIntegeral;
};
} // namespace Rendering::Passes
