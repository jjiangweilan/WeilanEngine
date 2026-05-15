#include "ShadingPass.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
static Gfx::PipelineConfig::PipelineConfig_t MakeStencilReadConfig(
    const Gfx::PipelineConfig& baseConfig,
    uint32_t reference
)
{
    auto config = *baseConfig;
    config.stencil.testEnable = true;
    config.stencil.front.failOp = Gfx::StencilOp::Keep;
    config.stencil.front.passOp = Gfx::StencilOp::Keep;
    config.stencil.front.depthFailOp = Gfx::StencilOp::Keep;
    config.stencil.front.compareOp = Gfx::CompareOp::Equal;
    config.stencil.front.compareMask = 0xFF;
    config.stencil.front.writeMask = 0;
    config.stencil.front.reference = reference;
    config.stencil.back = config.stencil.front;
    return config;
}

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

    grassLightingShader = ShaderLibrary::GetShader(Shaders::GrassLighting);
    grassLightingResource = GetGfxDriver()->CreateShaderResource();
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
    Gfx::ImageView* pointLightShadowMap,
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
    if (pointLightShadowMap)
        gpuResource->SetImage("pointLightShadowMap"_shaderBinding, pointLightShadowMap);

    cmd.BindResource(1, gpuResource.get());
    cmd.BindShaderProgram(shadingShader, MakeStencilReadConfig(*shadingShader->GetDefaultShaderConfig(), 1));
    cmd.Draw(6, 1, 0, 0);
}

void ShadingPass::ExecuteGrassLighting(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& albedoGBuffer
)
{
    auto grassLightingProgram = grassLightingShader->GetShaderProgram();
    int materialSet = grassLightingShader->GetSet(Gfx::DescriptorSetSemantics::Material);

    grassLightingResource->SetImage("albedoTex"_shaderBinding, albedoGBuffer);
    cmd.BindResource(materialSet, grassLightingResource.get());
    cmd.BindShaderProgram(grassLightingProgram, MakeStencilReadConfig(*grassLightingProgram->GetDefaultShaderConfig(), 2));
    cmd.Draw(6, 1, 0, 0);
}

void ShadingPass::UploadGPUParameter(
    float4 shadowMapTexelSize,
    float shadowConstantBias,
    float shadowNormalBias,
    int pointLightShadowLightIndex,
    float pointLightShadowFarPlane,
    float pointLightShadowDepthBias,
    glm::vec3 pointLightShadowLightPos
)
{
    cpuParameter = GPUParameter::DeferredPBRShadingInput{
        .shadowMapTexelSize = shadowMapTexelSize,
        .shadowConstantBias = shadowConstantBias,
        .shadowNormalBias = shadowNormalBias,
        .pointLightShadowLightIndex = pointLightShadowLightIndex,
        .pointLightShadowFarPlane = pointLightShadowFarPlane,
        .pointLightShadowLightPosAndBias = {pointLightShadowLightPos.x, pointLightShadowLightPos.y, pointLightShadowLightPos.z, pointLightShadowDepthBias}
    };

    GetGfxDriver()->UploadBuffer(
        *perMaterialBuffer,
        (uint8_t*)&cpuParameter,
        sizeof(GPUParameter::DeferredPBRShadingInput)
    );
}

} // namespace Rendering::Passes
