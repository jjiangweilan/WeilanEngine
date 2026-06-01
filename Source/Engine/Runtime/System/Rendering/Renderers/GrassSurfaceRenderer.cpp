#include "GrassSurfaceRenderer.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include <algorithm>

GrassSurfaceRenderer::GrassSurfaceRenderer()
{
    grass = ShaderLibrary::GetShader(Shaders::Grass);
    grassMotionVector = ShaderLibrary::GetShader(Shaders::Grass_MotionVector);
    paramsSetIndex = grass->GetSet("params");
    motionVectorParamsSetIndex = grassMotionVector->GetSet("params");
    instanceBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("GrassSurfaceRenderer InstanceBuffer", PipelineGPUBufferUsage::Stoage);
    grassParamBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("GrassSurfaceRenderer GrassParam", PipelineGPUBufferUsage::Uniform);
}

GrassSurfaceRenderer::~GrassSurfaceRenderer()
{
    PipelineGPUBufferAllocator::ReturnBuffer(instanceBuffer);
    PipelineGPUBufferAllocator::ReturnBuffer(grassParamBuffer);
}

void GrassSurfaceRenderer::Draw(
    GrassSurface& grassSurface,
    Gfx::CommandBuffer& cmd,
    const Rendering::RenderingData& renderingData
)
{
    if (!PrepareDrawData(grassSurface, renderingData))
        return;

    const auto& group = grassSurface.grassPatchGroup;

    Gfx::ShaderProgram* shaderProgram = grass->GetShaderProgram();
    auto config = *shaderProgram->GetDefaultShaderConfig();
    config.stencil.testEnable = true;
    config.stencil.front.passOp = Gfx::StencilOp::Replace;
    config.stencil.front.compareOp = Gfx::CompareOp::Always;
    config.stencil.front.compareMask = 0xFF;
    config.stencil.front.writeMask = 0xFF;
    config.stencil.front.reference = 2;
    config.stencil.back = config.stencil.front;
    if (renderingData.renderPipelineSettings->debugDraw.wireframe)
    {
        config.polygonMode = Gfx::PolygonMode::Line;
    }

    BindGrassParams(grassSurface.grassPatchGroup, cmd, paramsSetIndex, renderingData);
    cmd.BindShaderProgram(shaderProgram, config);

    for (const auto& batch : batches)
    {
        PushConstant pushConstant{
            .albedo = float4(group.config.albedo, 1.0f),
            .instanceOffset = batch.instanceOffset,
            .scale = glm::max(group.config.scale, 0.01f),
        };
        cmd.SetPushConstant(shaderProgram, &pushConstant);

        for (auto& submesh : batch.mesh->GetSubmeshes())
        {
            cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
            cmd.BindVertexBuffer(submesh.GetGfxVertexBufferBindings(), 0);
            cmd.DrawIndexed(submesh.GetIndexCount(), batch.instanceCount, 0, 0, batch.instanceOffset);
        }
    }
}

void GrassSurfaceRenderer::DrawMotionVectors(
    GrassSurface& grassSurface,
    Gfx::CommandBuffer& cmd,
    const Rendering::RenderingData& renderingData
)
{
    if (!PrepareDrawData(grassSurface, renderingData))
        return;

    const auto& group = grassSurface.grassPatchGroup;
    Gfx::ShaderProgram* shaderProgram = grassMotionVector->GetShaderProgram();

    BindGrassParams(grassSurface.grassPatchGroup, cmd, motionVectorParamsSetIndex, renderingData);
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());

    for (const auto& batch : batches)
    {
        PushConstant pushConstant{
            .albedo = float4(group.config.albedo, 1.0f),
            .instanceOffset = batch.instanceOffset,
            .scale = glm::max(group.config.scale, 0.01f),
        };
        cmd.SetPushConstant(shaderProgram, &pushConstant);

        for (auto& submesh : batch.mesh->GetSubmeshes())
        {
            cmd.BindIndexBuffer(submesh.GetIndexBuffer(), 0, submesh.GetIndexBufferType());
            cmd.BindVertexBuffer(submesh.GetGfxVertexBufferBindings(), 0);
            cmd.DrawIndexed(submesh.GetIndexCount(), batch.instanceCount, 0, 0, batch.instanceOffset);
        }
    }
}

bool GrassSurfaceRenderer::PrepareDrawData(GrassSurface& grassSurface, const Rendering::RenderingData& renderingData)
{
    if (!grassSurface.IsActiveInScene())
        return false;

    const auto& group = grassSurface.grassPatchGroup;
    if (group.patches.empty() || group.patchMeshes.empty())
        return false;

    struct PatchToDraw
    {
        Mesh* mesh = nullptr;
        const GrassPatch* patch = nullptr;
    };

    std::vector<PatchToDraw> patchesToDraw;
    patchesToDraw.reserve(group.patches.size());
    for (const auto& patch : group.patches)
    {
        if (patch.meshIndex < 0 || patch.meshIndex >= group.patchMeshes.size())
            continue;

        Mesh* mesh = group.patchMeshes[patch.meshIndex].Get();
        if (mesh == nullptr || mesh->GetSubmeshes().empty())
            continue;

        patchesToDraw.push_back({mesh, &patch});
    }

    if (patchesToDraw.empty())
        return false;

    std::sort(
        patchesToDraw.begin(), patchesToDraw.end(),
        [](const PatchToDraw& a, const PatchToDraw& b)
        {
            return a.mesh < b.mesh;
        }
    );

    instances.clear();
    instances.reserve(patchesToDraw.size());
    batches.clear();

    for (const auto& patchToDraw : patchesToDraw)
    {
        if (batches.empty() || batches.back().mesh != patchToDraw.mesh)
        {
            batches.push_back({patchToDraw.mesh, static_cast<uint32_t>(instances.size()), 0});
        }

        instances.push_back({patchToDraw.patch->position, 0.0f});
        batches.back().instanceCount++;
    }

    renderingData.pipelineAllocator->AllocateBuffer(instanceBuffer, instances.size() * sizeof(GrassPatchInstanceData));
    instanceBuffer.Write(instances.data(), instances.size() * sizeof(GrassPatchInstanceData));

    GrassParam grassParam;
    grassParam.grassColorRamp_Bottom = group.config.grassColorRamp_Bottom;
    grassParam.grassColorRamp_Top = group.config.grassColorRamp_Top;
    grassParam.grassColorRamp2_Bottom = group.config.grassColorRamp2_Bottom;
    grassParam.grassColorRamp2_Top = group.config.grassColorRamp2_Top;
    grassParam.grassColorRamp3_Bottom = group.config.grassColorRamp3_Bottom;
    grassParam.grassColorRamp3_Top = group.config.grassColorRamp3_Top;
    grassParam.grassMaskUVScaler = group.config.grassMaskUVScaler;
    grassParam.hueShift_0 = group.config.hueShift_0;
    grassParam.hueShift_1 = group.config.hueShift_1;
    grassParam.windScale = group.config.windScale;
    renderingData.pipelineAllocator->AllocateBuffer(grassParamBuffer, sizeof(GrassParam));
    grassParamBuffer.Write((void*)&grassParam, sizeof(GrassParam));

    return true;
}

void GrassSurfaceRenderer::BindGrassParams(GrassPatchGroup& group, Gfx::CommandBuffer& cmd, int setIndex, const Rendering::RenderingData& renderingData)
{
    std::vector<Gfx::DynamicBinding> paramsBindings = {
        Gfx::DynamicBinding("instanceData", *instanceBuffer.GetBuffer()),
        Gfx::DynamicBinding("params", *grassParamBuffer.GetBuffer()),
    };
    if (group.config.grassShadowMask0 != nullptr)
    {
        auto image = group.config.grassShadowMask0->GetGfxImage();
        paramsBindings.push_back(Gfx::DynamicBinding("grassShadowMask0", *image));
    }
    if (group.config.grassShadowMask1 != nullptr)
    {
        auto image = group.config.grassShadowMask1->GetGfxImage();
        paramsBindings.push_back(Gfx::DynamicBinding("grassShadowMask1", *image));
    }
    paramsBindings.push_back(Gfx::DynamicBinding("blueNoise", *renderingData.blueNoise.GetNoiseTexture()));
    if (group.config.windTex != nullptr)
    {
        auto image = group.config.windTex->GetGfxImage();
        paramsBindings.push_back(Gfx::DynamicBinding("windMask", *image));
    }

    cmd.BindResource(setIndex, paramsBindings);
}
