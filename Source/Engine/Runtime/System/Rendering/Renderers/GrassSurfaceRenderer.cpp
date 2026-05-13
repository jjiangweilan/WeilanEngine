#include "GrassSurfaceRenderer.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include <algorithm>

GrassSurfaceRenderer::GrassSurfaceRenderer()
{
    grass = ShaderLibrary::GetShader(Shaders::Grass);
    paramsSetIndex = grass->GetSet("params");
    instanceBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("GrassSurfaceRenderer InstanceBuffer", PipelineGPUBufferUsage::Stoage);
    lightingInputBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("GrassSurfaceRenderer LightingInput", PipelineGPUBufferUsage::Uniform);
}

GrassSurfaceRenderer::~GrassSurfaceRenderer()
{
    PipelineGPUBufferAllocator::ReturnBuffer(lightingInputBuffer);
    PipelineGPUBufferAllocator::ReturnBuffer(instanceBuffer);
}

void GrassSurfaceRenderer::Draw(
    GrassSurface& grassSurface,
    Gfx::CommandBuffer& cmd,
    const Rendering::RenderingData& renderingData,
    Gfx::ImageView* shadowMap,
    const Gfx::ImageIdentifier& contactShadowMap,
    Gfx::ImageView* pointLightShadowMap,
    const GPUParameter::DeferredPBRShadingInput& lightingInput
)
{
    if (!grassSurface.IsActiveInScene())
        return;

    const auto& group = grassSurface.grassPatchGroup;
    if (group.patches.empty() || group.patchMeshes.empty())
        return;

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
        return;

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
    grassParam.input = lightingInput;
    grassParam.grassColorRamp_Bottom = group.config.grassColorRamp_Bottom;
    grassParam.grassColorRamp_Top = group.config.grassColorRamp_Top;
    grassParam.grassColorRamp2_Bottom = group.config.grassColorRamp2_Bottom;
    grassParam.grassColorRamp2_Top = group.config.grassColorRamp2_Top;
    grassParam.grassColorRamp3_Bottom = group.config.grassColorRamp3_Bottom;
    grassParam.grassColorRamp3_Top = group.config.grassColorRamp3_Top;
    grassParam.grassMaskUVScaler = group.config.grassMaskUVScaler;
    grassParam.hueShift_0 = group.config.hueShift_0;
    grassParam.hueShift_1 = group.config.hueShift_1;
    renderingData.pipelineAllocator->AllocateBuffer(lightingInputBuffer, sizeof(GrassParam));
    lightingInputBuffer.Write((void*)&grassParam, sizeof(GrassParam));

    Gfx::ShaderProgram* shaderProgram = grass->GetShaderProgram();
    auto config = *shaderProgram->GetDefaultShaderConfig();
    if (renderingData.renderPipelineSettings->debugDraw.wireframe)
    {
        config.polygonMode = Gfx::PolygonMode::Line;
    }

    std::vector<Gfx::DynamicBinding> paramsBindings = {
        Gfx::DynamicBinding("instanceData", *instanceBuffer.GetBuffer()),
        Gfx::DynamicBinding("params", *lightingInputBuffer.GetBuffer()),
        Gfx::DynamicBinding("shadowMap", *shadowMap),
        Gfx::DynamicBinding("contactShadowMap", contactShadowMap),
    };
    if (pointLightShadowMap != nullptr)
    {
        paramsBindings.push_back(Gfx::DynamicBinding("pointLightShadowMap", *pointLightShadowMap));
    }
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

    cmd.BindResource(paramsSetIndex, paramsBindings);
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
