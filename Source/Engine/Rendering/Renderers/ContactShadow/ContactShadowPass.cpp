#include "ContactShadowPass.hpp"
#include "Shaders/ContactShadow/ContactShadowParameters.hlsl"

namespace Rendering
{

ContactShadowPass::ContactShadowPass()
{
    shader = ShaderLibrary::GetShader(Shaders::ContactShadow);
    mat.SetShader(shader);
}

void ContactShadowPass::Execute(
    Gfx::CommandBuffer& cmd, RenderingData& renderingData, Light* mainLight, Gfx::Image* depthTex
)
{
    if (!mainLight)
        return;

    auto* camera = renderingData.mainCamera;
    if (!camera)
        return;

    // Build light projection (directional only). If not directional, skip.
    if (mainLight->GetLightType() != LightType::Directional)
        return;

    // Acquire view-projection
    const float4x4& vp = renderingData.gpuCamera->viewProjection;

    float3 dir = mainLight->GetLightDirection(); // assumed normalized
    float4 lightProj = vp * float4(dir, 0.0f);

    int viewport[2] = {(int)renderingData.gpuCamera->screenSize.x, (int)renderingData.gpuCamera->screenSize.y};

    int minBounds[2] = {0, 0};
    int maxBounds[2] = {viewport[0], viewport[1]};

    Bend::DispatchList list = Bend::BuildDispatchList(&lightProj[0], viewport, minBounds, maxBounds);
    if (list.DispatchCount == 0)
        return;

    desc.SetWidth(viewport[0]);
    desc.SetHeight(viewport[1]);
    desc.SetFormat(Gfx::GfxFormat::R8_UNorm);
    desc.SetRandomWrite(true);
    cmd.AllocateAttachment(outputId, desc);

    auto outputImage = GetGfxDriver()->GetImageFromRenderGraph(outputId);
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

    Gfx::ClearColor clear;
    clear.float32[0] = 1.0f;
    clear.float32[1] = 1.0f;
    clear.float32[2] = 1.0f;
    clear.float32[3] = 1.0f;
    cmd.ClearColorImage(outputImage, clear);

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
