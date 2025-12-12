#include "OceanRenderer.hpp"
#include "Rendering/GeometryUtils.hpp"
#include "Rendering/ShaderLibrary.hpp"

OceanRenderer::OceanRenderer()
{
    oceanPatchShader = ShaderLibrary::GetShader(Shaders::OceanPatchShader);
    oceanParamsSetIndex = oceanPatchShader->GetSet("params");
    oceanMaterialSetIndex = oceanPatchShader->GetSet("mat");

    instanceBuffer.buffer = PipelineGPUBufferAllocator::RequestGPUBuffer("OceanRenderer InstanceBuffer", PipelineGPUBufferUsage::Stoage);
}

OceanRenderer::~OceanRenderer()
{
    PipelineGPUBufferAllocator::ReturnBuffer(instanceBuffer.buffer);
}

void OceanRenderer::Setup(std::span<int> vertexSize)
{
    InitPatchLodData(vertexSize);
}

void OceanRenderer::Render(Gfx::CommandBuffer& cmd, OceanQuadTree& quadTree, Material& waveMaterial, const Rendering::RenderingData& renderingData)
{
    cmd.BeginLabel("Ocean Rendering", float4(0.05f, 0.865f, 0.345, 1.0f));
    auto& quadTreeInfo = quadTree.GetQuadTreeInfo();

    int totalInstance = 0;
    for (int lodLevel = 0; lodLevel < quadTreeInfo.mipLevels; lodLevel++)
    {
        auto& nodes = quadTree.GetNodesAtLOD(lodLevel);
        totalInstance += nodes.size();
    }

    EnsureInstanceBufferSize(totalInstance, renderingData);
    FillInstanceData(quadTree);
    DrawPatches(cmd, waveMaterial, renderingData);
    cmd.EndLabel();
}

void OceanRenderer::InitPatchLodData(std::span<int> vertexSize)
{
    patchLodDatas.clear();
    for (int i = 0; i < vertexSize.size(); ++i)
    {
        PatchDesc patchDesc = {
            .vertices = vertexSize[i],
        };

        PatchLodData lodData = {.desc = patchDesc, .patch = Rendering::GeneratePlane(patchDesc.meter, patchDesc.meter, patchDesc.vertices, patchDesc.vertices, false), .instanceDataOffset = 0, .instanceCount = 0};
        patchLodDatas.push_back(std::move(lodData));
    }
}

void OceanRenderer::EnsureInstanceBufferSize(size_t count, const Rendering::RenderingData& renderingData)
{
    if (instanceBuffer.count < count)
    {
        instanceBuffer.cpuData.resize(count);
    }

    instanceBuffer.count = count;
    renderingData.pipelineAllocator->AllocateBuffer(instanceBuffer.buffer, sizeof(GPUNodeInstanceData) * count);
}

void OceanRenderer::DrawPatches(Gfx::CommandBuffer& cmd, Material& waveMaterial, const Rendering::RenderingData& renderingData)
{
    // Prepare patchRenderShaderResource //
    auto rendererInputUBOVal = *rendererInputUBO.GetPtr();
    auto& depthTexDescription = renderingData.depthCopy->GetDescription();
    rendererInputUBOVal.depthTexSize = {
        depthTexDescription.width,
        depthTexDescription.height,
        1.0f / depthTexDescription.width,
        1.0f / depthTexDescription.height
    };
    rendererInputUBO.SetAndUpload(rendererInputUBOVal);

    auto shader = oceanPatchShader->GetShaderProgram();
    auto renderPipelineSettings = renderingData.renderPipelineSettings;

    cmd.BindResource(oceanMaterialSetIndex, waveMaterial.GetShaderResource());
    // cmd.BindResource(oceanParamsSetIndex, patchRenderShaderResource.get());

    cmd.BindResource(
        oceanParamsSetIndex,
        {
            {"buffer", &*rendererInputUBO},
            {"instanceData", instanceBuffer.buffer.GetBuffer()},
            {"depthTex", *renderingData.depthCopy},
            {"colorTex", *renderingData.colorCopy},
            {"specularCubemap", *renderingData.specularCubemap},
        }
    );
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

    instanceBuffer.buffer.Write(instanceBuffer.cpuData.data(), instanceBuffer.count * sizeof(GPUNodeInstanceData));
}
