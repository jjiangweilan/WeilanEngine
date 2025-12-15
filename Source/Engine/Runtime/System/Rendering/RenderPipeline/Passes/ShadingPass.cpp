#include "ShadingPass.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
ShadingPass::ShadingPass()
{
    gpuResource = GetGfxDriver()->CreateShaderResource();
    perMaterialBuffer = GetGfxDriver()->CreateBuffer(
        sizeof(GPUParameter::DeferredPBRShadingInput),
        Gfx::BufferUsage::Uniform,
        false,
        false,
        "Deferred PBR Shading Input"
    );
    gpuResource->SetBuffer("perMaterial", perMaterialBuffer.get());
    brdfPreIntegeral = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/BRDFPreintegral.ktx");
    gpuResource->SetImage("specularBRDFIntegrationMap", brdfPreIntegeral->GetGfxImage());
    shadingShader = ShaderLibrary::GetShader(Shaders::DeferredPBRShading);
}

void ShadingPass::OnInit(RenderingData* renderingData)
{
}

void ShadingPass::Execute(
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
)
{
    auto shadingShader = this->shadingShader->GetShaderProgram();
    
    gpuResource->SetImage("contactShadowMap"_shaderBinding, *contactShadowMap);
    gpuResource->SetImage("albedoTex"_shaderBinding, albedoGBuffer);
    gpuResource->SetImage("normalTex"_shaderBinding, normalGBuffer);
    gpuResource->SetImage("maskTex"_shaderBinding, maskGBuffer);
    gpuResource->SetImage("depthTex"_shaderBinding, depthImageView);
    gpuResource->SetImage("shadowMap"_shaderBinding, shadowMap);
    gpuResource->SetImage("ambientOcclusion"_shaderBinding, *ssaoTex);
    if (diffuseCube)
        gpuResource->SetImage("diffuseCube"_shaderBinding, diffuseCube);
    if (specularCube)
        gpuResource->SetImage("specularCube"_shaderBinding, specularCube);

    cmd.BindResource(1, gpuResource.get());
    cmd.BindShaderProgram(shadingShader, shadingShader->GetDefaultShaderConfig());
    cmd.Draw(6, 1, 0, 0);
}

void ShadingPass::UploadGPUParameter(
    float4 shadowMapTexelSize,
    float shadowConstantBias,
    float shadowNormalBias
)
{
    cpuParameter = GPUParameter::DeferredPBRShadingInput{
        .shadowMapTexelSize = shadowMapTexelSize,
        .shadowConstantBias = shadowConstantBias,
        .shadowNormalBias = shadowNormalBias
    };

    GetGfxDriver()->UploadBuffer(
        *perMaterialBuffer,
        (uint8_t*)&cpuParameter,
        sizeof(GPUParameter::DeferredPBRShadingInput)
    );
}

} // namespace Rendering::Passes
