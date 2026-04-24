#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"

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
        Gfx::ImageView* pointLightShadowMap,
        RenderingData& renderingData
    );

    void UploadGPUParameter(
        float4 shadowMapTexelSize,
        float shadowConstantBias,
        float shadowNormalBias,
        int pointLightShadowLightIndex,
        float pointLightShadowFarPlane,
        float pointLightShadowDepthBias,
        glm::vec3 pointLightShadowLightPos
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
