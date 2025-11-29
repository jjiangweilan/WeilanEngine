#include "OceanRenderer.hpp"
#include "Rendering/GeometryUtils.hpp"
#include "Rendering/ShaderLibrary.hpp"

OceanRenderer::OceanRenderer()
{
}

void OceanRenderer::Setup()
{
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
    EnsurePatchLodData(quadTree);
    FillInstanceData(quadTree);
    DrawPatches(cmd, renderingData);
}

void OceanRenderer::EnsurePatchLodData(OceanQuadTree& quadTree)
{
    auto& quadTreeInfo = quadTree.GetQuadTreeInfo();

    if (patchLodDatas.size() < quadTreeInfo.mipLevels)
    {
        patchLodDatas.clear();
        for (int i = 0; i < quadTreeInfo.mipLevels; ++i)
        {
            PatchDesc patchDesc = {
                .vertices = glm::max((256 >> (2 * i)) + 1, 5),
            };

            PatchLodData lodData = {.desc = patchDesc, .patch = Rendering::GeneratePlane(patchDesc.meter, patchDesc.meter, patchDesc.vertices, patchDesc.vertices, false), .instanceDataOffset = 0, .instanceCount = 0};
            patchLodDatas.push_back(std::move(lodData));
        }
    }
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
    for (auto& patchLodData : patchLodDatas)
    {
        auto submesh = patchLodData.patch->GetSubmesh(0);

        cmd.BindIndexBuffer(submesh->GetIndexBuffer(), 0, submesh->GetIndexBufferType());
        cmd.BindVertexBuffer(submesh->GetGfxVertexBufferBindings(), 0);
        cmd.DrawIndexed(submesh->GetIndexCount(), patchLodData.instanceCount, 0, 0, patchLodData.instanceDataOffset);
    }
}

void OceanRenderer::FillInstanceData(OceanQuadTree& quadTree)
{
    auto& quadTreeInfo = quadTree.GetQuadTreeInfo();

    int nodeIdx = 0;
    for (int lodLevel = 0; lodLevel < quadTreeInfo.mipLevels; lodLevel++)
    {
        auto& nodes = quadTree.GetNodesAtLOD(lodLevel);

        patchLodDatas[lodLevel].instanceDataOffset = nodeIdx;
        patchLodDatas[lodLevel].instanceCount = nodes.size();

        for (auto& node : nodes)
        {
            instanceBuffer.cpuData[nodeIdx].position = node->minPos;
            instanceBuffer.cpuData[nodeIdx].scale = (node->maxPos - node->minPos) / PatchDesc::meter;
            nodeIdx++;
        }
    }

    GetGfxDriver()->UploadBuffer(
        *instanceBuffer.buffer,
        (uint8_t*)instanceBuffer.cpuData.data(),
        instanceBuffer.cpuData.size() * sizeof(GPUNodeInstanceData)
    );
}
