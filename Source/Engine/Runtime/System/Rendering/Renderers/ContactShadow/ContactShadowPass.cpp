#include "ContactShadowPass.hpp"
#include "MiddleLayer/EngineInternalResources.hpp"
#include "Shaders/ContactShadow/ContactShadowParameters.hlsl"

namespace Rendering
{

ContactShadowPass::ContactShadowPass()
{
    shader = ShaderLibrary::GetShader(Shaders::ContactShadow);
    mat.SetShader(shader);
}

Gfx::ImageIdentifier& ContactShadowPass::GetOutputId()
{
    return outputID;
}

void ContactShadowPass::Execute(
    Gfx::CommandBuffer& cmd, RenderingData& renderingData, Light* mainLight, Gfx::Image* depthTex
)
{
    valid = renderingData.renderPipelineSettings->contactShadow.enabled && mainLight && mainLight->GetLightType() == LightType::Directional;

    if (!valid)
    {
        outputID = *EngineInternalResources::GetWhiteTexture().GetGfxImage();
        return;
    }
    outputID = contactShadowMap;

    // Acquire view-projection
    float4x4 p = renderingData.gpuCamera->projection;
    float4x4 v = renderingData.gpuCamera->view;
    p[1] = -p[1]; // Seems Bend's algorithm is expecting a left hand NDC space
    const auto& vp = p * v;

    float3 dir = -mainLight->GetLightDirection(); // assumed normalized
    float4 lightProj = vp * float4(dir, 0.0f);

    int viewport[2] = {(int)renderingData.gpuCamera->screenSize.x, (int)renderingData.gpuCamera->screenSize.y};

    int minBounds[2] = {0, 0};
    int maxBounds[2] = {viewport[0], viewport[1]};

    Bend::DispatchList list = Bend::BuildDispatchList(&lightProj[0], viewport, minBounds, maxBounds);
    if (list.DispatchCount == 0)
        return;

    desc.SetWidth(viewport[0]);
    desc.SetHeight(viewport[1]);
    desc.SetFormat(Gfx::GfxFormat::R32_SFloat);
    desc.SetRandomWrite(true);
    cmd.AllocateAttachment(contactShadowMap, desc);

    auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(contactShadowMap);
    mat.SetTexture("DepthTexture", depthTex);
    mat.SetTexture("OutputTexture", outputImage);
    mat.SetVector(
        "lightCoord",
        float4(
            list.LightCoordinate_Shader[0],
            list.LightCoordinate_Shader[1],
            list.LightCoordinate_Shader[2],
            list.LightCoordinate_Shader[3]
        )
    );
    mat.SetFloat("farDepthValue", 0.0f);
    mat.SetFloat("nearDepthValue", 1.0f);
    mat.SetVector(
        "invDepthTextureSize",
        float4(1.0f / depthTex->GetDescription().width, 1.0f / depthTex->GetDescription().height, 0, 0)
    );
    mat.SetFloat("thickness", renderingData.renderPipelineSettings->contactShadow.thickness);

    // TODO: set PointBorderSampler if explicit binding required (engine default sampler might suffice)

    cmd.BeginLabel("ContactShadow", {0.15f, 0.15f, 0.4f, 1.0f});

    // For each dispatch configure push constants (WaveOffset + LightCoordinate)
    for (int i = 0; i < list.DispatchCount; ++i)
    {
        ContactShadowPushConstant ps;
        ps.waveOffset = {list.Dispatch[i].WaveOffset_Shader[0], list.Dispatch[i].WaveOffset_Shader[1]};

        auto* prog = shader->GetShaderProgram();
        cmd.BindResource(0, mat.GetShaderResource());
        cmd.BindShaderProgram(prog, prog->GetDefaultShaderConfig());
        cmd.SetPushConstant(prog, &ps);

        // Dispatch: (x,y,z) = (waveCountX, waveCountY, waveCountZ)
        cmd.Dispatch(list.Dispatch[i].WaveCount[0], list.Dispatch[i].WaveCount[1], list.Dispatch[i].WaveCount[2]);
    }

    cmd.EndLabel();
}
} // namespace Rendering
