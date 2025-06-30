#include "ReflectionProbeUpdate.hpp"
#include "Core/Component/ReflectionProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderingUtils.hpp"

namespace Rendering::Passes
{

ReflectionProbeUpdate::ReflectionProbeUpdate(Gfx::Buffer* sceneBuffer, Gfx::Buffer* mainLightShadowBuffer)
{
    mainColorDescription = Gfx::RG::ImageDescription(1, 1, Gfx::GfxFormat::R8G8B8A8_SRGB, false);
    albedoImageDescription = Gfx::RG::ImageDescription(1, 1, Gfx::GfxFormat::R8G8B8A8_SRGB, false);
    normalImageDescription = Gfx::RG::ImageDescription(1, 1, Gfx::GfxFormat::A2B10G10R10_UNorm, false);
    maskImageDescription = Gfx::RG::ImageDescription(1, 1, Gfx::GfxFormat::R8G8B8A8_UNorm, false);
    depthImageDescription = Gfx::RG::ImageDescription(1, 1, Gfx::GfxFormat::D32_SFloat, false);

    Gfx::RG::SubpassAttachment lighting{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store};
    Gfx::RG::SubpassAttachment albedo{1};
    Gfx::RG::SubpassAttachment normal{2};
    Gfx::RG::SubpassAttachment property{3};
    Gfx::RG::SubpassAttachment depth{4};
    Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
    gbufferPass = Gfx::RG::RenderPass(1, 5);
    gbufferPass.SetSubpass(0, subpassAttachments, depth);

    for (int i = 0; i < 6; ++i)
    {
        faceBuffers[i] = GetGfxDriver()->CreateBuffer(
            sizeof(GPUParameter::Camera),
            Gfx::BufferUsage::Uniform,
            false,
            false,
            "Reflection Probe Face"
        );
        faceResources[i] = GetGfxDriver()->CreateShaderResource();
        faceResources[i]->SetBuffer("scene", sceneBuffer);
        faceResources[i]->SetBuffer("camera", faceBuffers[i].get());
        faceResources[i]->SetBuffer("mainLightShadow", mainLightShadowBuffer);
    }
}

void ReflectionProbeUpdate::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData, ReflectionProbe& probe)
{
    auto& renderingScene = renderingData.scene->GetRenderingScene();
    auto renderers = renderingScene.QueryRendererInFrustum(probe.GetFrustum(0));

    float3 probeWorldPosition = float3(0);

    if (probe.GetUpdateType() == ReflectionProbe::UpdateType::Local)
    {
        probeWorldPosition = probe.GetGameObject()->GetPosition();
    }

    for (int i = 0; i < 6; ++i)
    {
        mainColorDescription.SetWidth(reflectionProbeSize);
        mainColorDescription.SetHeight(reflectionProbeSize);
        albedoImageDescription.SetWidth(reflectionProbeSize);
        albedoImageDescription.SetHeight(reflectionProbeSize);
        normalImageDescription.SetWidth(reflectionProbeSize);
        normalImageDescription.SetHeight(reflectionProbeSize);
        maskImageDescription.SetWidth(reflectionProbeSize);
        maskImageDescription.SetHeight(reflectionProbeSize);
        depthImageDescription.SetWidth(reflectionProbeSize);
        depthImageDescription.SetHeight(reflectionProbeSize);

        cmd.BeginLabel("Reflection Probe Update", float4(0.23, 0.112, 0.65, 1.0));
        cmd.AllocateAttachment(mainColor, mainColorDescription);
        cmd.AllocateAttachment(albedo, albedoImageDescription);
        cmd.AllocateAttachment(normal, normalImageDescription);
        cmd.AllocateAttachment(mask, maskImageDescription);
        cmd.AllocateAttachment(depth, depthImageDescription);

        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};
        gbufferPass.SetAttachment(0, mainColor);
        gbufferPass.SetAttachment(1, albedo);
        gbufferPass.SetAttachment(2, normal);
        gbufferPass.SetAttachment(3, mask);
        gbufferPass.SetAttachment(4, depth);
        cmd.BeginRenderPass(gbufferPass, clears);

        DrawList drawList{};
        drawList.Add(renderers);
        drawList.DrawRangeHelper(cmd, 0, drawList.opaqueIndex);

        cmd.EndRenderPass();
        cmd.EndLabel(); // "Reflection Probe Update"
    }
}
} // namespace Rendering::Passes
