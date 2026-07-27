#include "MotionVectorPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/Renderers/GrassSurfaceRenderer.hpp"

namespace Rendering::Passes
{
MotionVectorPass::MotionVectorPass()
{
    staticShader = ShaderLibrary::GetShader(Shaders::PostProcess_StaticMotionVector);
    staticMaterial.SetShader(staticShader);

    dynamicShader = ShaderLibrary::GetShader(Shaders::PostProcess_DynamicMotionVector);
    treeMotionShader = ShaderLibrary::GetShader(Shaders::TreeMotionVector);
    treeDynamicMotionShader = ShaderLibrary::GetShader(Shaders::TreeMotionVector, {"_ObjectMotion"});

    Gfx::SubpassAttachment attachments[] = {
        {0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    };
    staticRenderPass.SetSubpass(0, attachments);
}

void MotionVectorPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& depth,
    const Gfx::RenderImageDescriptor& depthDesc,
    const RenderingData& renderingData,
    Gfx::Buffer* indirectCommandBuffer,
    std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
    uint32_t dynamicMotionDataOffset,
    GrassSurfaceRenderer* grassSurfaceRenderer,
    std::span<GrassSurface*> grassSurfaces
)
{
    DrawStaticMotionVectors(cmd, depth, depthDesc);
    DrawDynamicAndGrassMotionVectors(
        cmd,
        depth,
        renderingData,
        indirectCommandBuffer,
        gpuObjectShaderGroups,
        dynamicMotionDataOffset,
        grassSurfaceRenderer,
        grassSurfaces
    );

    if (renderingData.renderPipelineSettings)
        debugView = renderingData.renderPipelineSettings->debugDraw.motionVectors;
}

void MotionVectorPass::DrawStaticMotionVectors(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& depth,
    const Gfx::RenderImageDescriptor& depthDesc
)
{
    cmd.BeginLabel("MotionVector Static", {0.1f, 0.5f, 0.5f, 1.0f});

    Gfx::RenderImageDescriptor velocityDesc(
        depthDesc.GetWidth(),
        depthDesc.GetHeight(),
        Gfx::GfxFormat::R16G16_SFloat
    );
    cmd.AllocateAttachment(motionVector, velocityDesc);

    auto depthImage = GetGfxDriver()->GetImageFromRenderGraph(depth);
    staticMaterial.SetTexture("depthTex", depthImage);

    staticRenderPass.SetAttachment(0, motionVector);
    Gfx::ClearValue clear[] = {{0.0f, 0.0f, 0.0f, 0.0f}};
    cmd.BeginRenderPass(staticRenderPass, clear);

    auto shaderProgram = staticMaterial.GetShaderProgram();
    cmd.BindResource(staticMaterial.GetSet(Gfx::DescriptorSetSemantics::Material), staticMaterial.GetShaderResource());
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());
    cmd.Draw(6, 1, 0, 0);

    cmd.EndRenderPass();
    cmd.EndLabel();
}

void MotionVectorPass::DrawDynamicAndGrassMotionVectors(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& depth,
    const RenderingData& renderingData,
    Gfx::Buffer* indirectCommandBuffer,
    std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
    uint32_t dynamicMotionDataOffset,
    GrassSurfaceRenderer* grassSurfaceRenderer,
    std::span<GrassSurface*> grassSurfaces
)
{
    bool hasIndirectDraws = indirectCommandBuffer && !gpuObjectShaderGroups.empty();
    bool hasDynamic = dynamicMotionDataOffset != InvalidTextureIndex && hasIndirectDraws;
    bool hasTree = false;
    if (hasIndirectDraws)
    {
        for (const auto& group : gpuObjectShaderGroups)
        {
            if (group.shaderProgram->GetName() == ShaderLibrary::GetShaderName(Shaders::TreeSceneLit))
            {
                hasTree = true;
                break;
            }
        }
    }
    bool hasGrass = grassSurfaceRenderer != nullptr && !grassSurfaces.empty();

    if (!hasDynamic && !hasTree && !hasGrass)
        return;

    cmd.BeginLabel("MotionVector Dynamic", {0.1f, 0.7f, 0.7f, 1.0f});

    Gfx::RenderAttachment attachments[] = {
        {motionVector, Gfx::AttachmentLoadOperation::Load},
        {depth, Gfx::AttachmentLoadOperation::Load, Gfx::AttachmentStoreOperation::Store, Gfx::AttachmentLoadOperation::Load, Gfx::AttachmentStoreOperation::Store},
    };
    Gfx::ClearValue clears[] = {{0.0f, 0.0f, 0.0f, 0.0f}, {0, 0}};
    cmd.BeginRenderPass(attachments, clears);

    // GPU-driven object and procedural tree motion vectors.
    if (hasIndirectDraws && (hasDynamic || hasTree))
    {
        cmd.BindIndexBuffer(GPUDrivenManager::Instance().GetGlobalBuffer(), 0, Gfx::IndexBufferType::UInt32);

        struct PushConstant
        {
            uint32_t firstGpuObjectOffset = 0;
            uint32_t firstDynamicMotionDataByteOffset = 0;
        } pconst;

        for (const auto& group : gpuObjectShaderGroups)
        {
            bool isTree = group.shaderProgram->GetName() == ShaderLibrary::GetShaderName(Shaders::TreeSceneLit);
            if (!group.hasMotion && !isTree)
                continue;

            Shader* motionShader = isTree
                                       ? (group.hasMotion
                                              ? treeDynamicMotionShader.Get()
                                              : treeMotionShader.Get())
                                       : dynamicShader.Get();
            auto* shaderProgram = motionShader->GetShaderProgram();
            cmd.BindResource(
                motionShader->GetSet(Gfx::DescriptorSetSemantics::Global),
                renderingData.globalResource
            );
            cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultPipelineConfig());

            pconst.firstGpuObjectOffset = group.firstDrawIndex;
            pconst.firstDynamicMotionDataByteOffset = group.hasMotion
                                                          ? dynamicMotionDataOffset + group.firstDynamicMotionDataIndex * sizeof(GPUDynamicMotionData)
                                                          : 0;
            cmd.SetPushConstant(shaderProgram, &pconst);
            cmd.DrawIndexedIndirect(
                indirectCommandBuffer,
                group.firstDrawIndex * sizeof(DrawIndexedIndirectCommand),
                group.drawCount,
                sizeof(DrawIndexedIndirectCommand)
            );
        }
    }

    // Grass motion vectors
    if (hasGrass)
    {
        for (auto* grassSurface : grassSurfaces)
        {
            grassSurfaceRenderer->DrawMotionVectors(*grassSurface, cmd, renderingData);
        }
    }

    cmd.EndRenderPass();
    cmd.EndLabel();
}

bool MotionVectorPass::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugView)
    {
        dst = motionVector;
        return true;
    }
    return false;
}
} // namespace Rendering::Passes
