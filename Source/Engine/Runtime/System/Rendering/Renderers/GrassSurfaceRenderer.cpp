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
}

GrassSurfaceRenderer::~GrassSurfaceRenderer()
{
    PipelineGPUBufferAllocator::ReturnBuffer(instanceBuffer);
}

void GrassSurfaceRenderer::Draw(GrassSurface& grassSurface, Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData)
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

    Gfx::ShaderProgram* shaderProgram = grass->GetShaderProgram();
    auto config = *shaderProgram->GetDefaultShaderConfig();
    if (renderingData.renderPipelineSettings->debugDraw.wireframe)
    {
        config.polygonMode = Gfx::PolygonMode::Line;
    }

    cmd.BindResource(paramsSetIndex, {Gfx::DynamicBinding("instanceData", *instanceBuffer.GetBuffer())});
    cmd.BindShaderProgram(shaderProgram, config);

    for (const auto& batch : batches)
    {
        PushConstant pushConstant{
            .instanceOffset = batch.instanceOffset,
            .albedo = group.config.albedo,
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
