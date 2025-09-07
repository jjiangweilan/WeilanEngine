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

    float3 dir = renderingData.gpuScene->lights[0].position; // assumed normalized
    float4 lightProj = vp * float4(dir, 0.0f);

    int viewport[2] = {(int)renderingData.gpuCamera->screenSize.x, (int)renderingData.gpuCamera->screenSize.y};

    int minBounds[2] = {0, 0};
    int maxBounds[2] = {viewport[0], viewport[1]};

    Bend::DispatchList list = Bend::BuildDispatchList(&lightProj[0], viewport, minBounds, maxBounds, false, 64);
    if (list.DispatchCount == 0)
        return;

    Gfx::RG::ImageDescription desc(viewport[0], viewport[1], Gfx::GfxFormat::R8_UNorm);
    desc.SetRandomWrite(true);
    cmd.AllocateAttachment(outputId, desc);
    cachedSize = {viewport[0], viewport[1]};

    // Bind static images
    mat.SetTexture("DepthTexture", depthTex);
    mat.SetTexture("OutputTexture", GetGfxDriver()->GetImageFromRenderGraph(outputId));
    // TODO: set PointBorderSampler if explicit binding required (engine default sampler might suffice)

    cmd.BeginLabel("ContactShadow", {0.15f, 0.15f, 0.4f, 1.0f});

    // For each dispatch configure push constants (WaveOffset + LightCoordinate)
    for (int i = 0; i < list.DispatchCount; ++i)
    {
        // Minimal parameter block (matches subset used early in shader). Layout must match shader expectation.
        mat.SetVector(
            "lightCoord",
            float4(
                list.LightCoordinate_Shader[0],
                list.LightCoordinate_Shader[1],
                list.LightCoordinate_Shader[2],
                list.LightCoordinate_Shader[3]
            )
        );
        mat.SetFloat("farDepth", camera->GetFar());
        mat.SetFloat("nearDepthValue", camera->GetNear());
        mat.SetVector(
            "invDepthTextureSize",
            float4(depthTex->GetDescription().width, depthTex->GetDescription().height, 0, 0)
        );

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
