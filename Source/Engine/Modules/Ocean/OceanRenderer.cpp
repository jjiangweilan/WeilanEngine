#include "OceanRenderer.hpp"
#include "Rendering/GeometryUtils.hpp"
#include "Rendering/ShaderLibrary.hpp"

OceanRenderer::OceanRenderer()
{
}

void OceanRenderer::Setup()
{
    patch = Rendering::GeneratePlane(meshDesc.meter, meshDesc.meter, meshDesc.vertices, meshDesc.vertices, false);
    oceanPatchShader = ShaderLibrary::GetShader(Shaders::OceanPatchShader);
    patchRenderShaderResource = GetGfxDriver()->CreateShaderResource();
}

void OceanRenderer::Render(Gfx::CommandBuffer& cmd, OceanQuadTree& quadTree, const Rendering::RenderingData& renderingData)
{
    auto& quadTreeInfo = quadTree.GetQuadTreeInfo();

    int totalInstance = 0;
    for (int lodLevel = 0; lodLevel < quadTreeInfo.mipLevels; lodLevel++)
    {
        auto& nodes = quadTree.GetNodesAtLOD(lodLevel);
        totalInstance += nodes.size();
    }

    EnsureInstanceBufferSize(totalInstance);
    FillInstanceData(quadTree);
    DrawPatches(cmd, renderingData);
}

void OceanRenderer::EnsureInstanceBufferSize(size_t count)
{
    if (instanceBuffer.count < count)
    {
        instanceBuffer.buffer = GetGfxDriver()->CreateBuffer(
            count * sizeof(GPUNodeInstanceData),
            Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
            false,
            false,
            "OceanRenderer InstanceBuffer"
        );
        instanceBuffer.cpuData.resize(count);
        instanceBuffer.count = count;
        patchRenderShaderResource->SetBuffer("instanceData", instanceBuffer.buffer.get());
    }
}

void OceanRenderer::DrawPatches(Gfx::CommandBuffer& cmd, const Rendering::RenderingData& renderingData)
{
    auto submesh = patch->GetSubmesh(0);
    auto shader = oceanPatchShader->GetShaderProgram();
    auto renderPipelineSettings = renderingData.renderPipelineSettings;

    cmd.BindResource(1, patchRenderShaderResource.get());
    if (renderPipelineSettings->debugDraw.wireframe)
    {
        auto config = *shader->GetDefaultShaderConfig();
        config.polygonMode = Gfx::PolygonMode::Line;
        cmd.BindShaderProgram(shader, config);
    }
    else
        cmd.BindShaderProgram(shader, shader->GetDefaultShaderConfig());
    cmd.BindIndexBuffer(submesh->GetIndexBuffer(), 0, submesh->GetIndexBufferType());
    cmd.BindVertexBuffer(submesh->GetGfxVertexBufferBindings(), 0);
    cmd.DrawIndexed(submesh->GetIndexCount(), instanceBuffer.count, 0, 0, 0);
}

void OceanRenderer::FillInstanceData(OceanQuadTree& quadTree)
{
    auto& quadTreeInfo = quadTree.GetQuadTreeInfo();

    int nodeIdx = 0;
    for (int lodLevel = 0; lodLevel < quadTreeInfo.mipLevels; lodLevel++)
    {
        auto& nodes = quadTree.GetNodesAtLOD(lodLevel);

        for (auto& node : nodes)
        {
            instanceBuffer.cpuData[nodeIdx].position = node->minPos;
            instanceBuffer.cpuData[nodeIdx].scale = (node->maxPos - node->minPos) / meshDesc.meter;
            nodeIdx++;
        }
    }

    GetGfxDriver()->UploadBuffer(
        *instanceBuffer.buffer,
        (uint8_t*)instanceBuffer.cpuData.data(),
        instanceBuffer.cpuData.size() * sizeof(GPUNodeInstanceData)
    );
}
