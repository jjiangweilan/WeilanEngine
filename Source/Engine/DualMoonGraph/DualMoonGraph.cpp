#pragma once
#include "DualMoonGraph.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderGraph/NodeBuilder.hpp"

namespace Engine
{
using namespace RenderGraph;

DualMoonGraph::DualMoonGraph(Scene& scene) : scene(scene)
{
    BuildGraph();
}

void DualMoonGraph::BuildGraph()
{
    std::vector<Gfx::ClearValue> clearValues = {{.color = {0, 0, 0, 0}}, {.depthStencil = {.depth = 0}}};
    auto swapChainProxy = GetGfxDriver()->GetSwapChainImageProxy().Get();
    uint32_t width = swapChainProxy->GetDescription().width;
    uint32_t height = swapChainProxy->GetDescription().height;
    auto forwardOpaque = AddNode(
        [this, clearValues](Gfx::CommandBuffer& cmd, Gfx::RenderPass& pass, const ResourceRefs& res)
        {
            for (auto& draw : this->drawList)
            {
                cmd.BeginRenderPass(&pass, clearValues);
                cmd.BindShaderProgram(draw.shader, *draw.shaderConfig);
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(draw.shaderResource);
                cmd.SetPushConstant(draw.shader, &draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                cmd.EndRenderPass();
            }
        },
        {
            {
                .name = "opaque color",
                .handle = 0,
                .type = ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Color_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                .imageLayout = Gfx::ImageLayout::Color_Attachment,
                .imageCreateInfo =
                    {
                        .width = width,
                        .height = height,
                        .format = Gfx::ImageFormat::R16G16B16A16_SFloat,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
            {
                .name = "opaque depth",
                .handle = 1,
                .type = ResourceType::Image,
                .accessFlags =
                    Gfx::AccessMask::Depth_Stencil_Attachment_Read | Gfx::AccessMask::Depth_Stencil_Attachment_Write,
                .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests,
                .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
                .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                .imageCreateInfo =
                    {
                        .width = width,
                        .height = height,
                        .format = Gfx::ImageFormat::D32_SFLOAT_S8_UInt,
                        .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                        .mipLevels = 1,
                        .isCubemap = false,
                    },
            },
        },
        {
            {
                .colors =
                    {
                        {
                            .handle = 0,
                            .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                            .loadOp = Gfx::AttachmentLoadOperation::Clear,
                            .storeOp = Gfx::AttachmentStoreOperation::Store,
                        },
                    },
                .depth = {{
                    .handle = 0,
                    .multiSampling = Gfx::MultiSampling::Sample_Count_1,
                    .loadOp = Gfx::AttachmentLoadOperation::Clear,
                    .storeOp = Gfx::AttachmentStoreOperation::Store,
                }},
            },
        }
    );

    auto blitToSwapchain = AddNode(
        [](Gfx::CommandBuffer& cmd, auto& pass, const ResourceRefs& res)
        {
            Gfx::Image* src = (Gfx::Image*)res.at(1)->GetResource();
            Gfx::Image* dst = (Gfx::Image*)res.at(0)->GetResource();
            cmd.Blit(src, dst);
        },
        {
            {
                .name = "swap chain image",
                .handle = 0,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Transfer_Write,
                .stageFlags = Gfx::PipelineStage::Transfer,
                .imageUsagesFlags = Gfx::ImageUsage::TransferDst,
                .imageLayout = Gfx::ImageLayout::Transfer_Dst,
                .externalImage = swapChainProxy,
            },
            {
                .name = "src",
                .handle = 1,
                .type = RenderGraph::ResourceType::Image,
                .accessFlags = Gfx::AccessMask::Transfer_Read,
                .stageFlags = Gfx::PipelineStage::Transfer,
                .imageUsagesFlags = Gfx::ImageUsage::TransferSrc,
                .imageLayout = Gfx::ImageLayout::Transfer_Src,
            },
        },
        {}
    );

    Connect(forwardOpaque, 0, blitToSwapchain, 1);

    colorOutput = blitToSwapchain;
    colorHandle = 0;
    depthOutput = forwardOpaque;
    depthHandle = 1;
};

void DualMoonGraph::AppendDrawData(Transform& transform, std::vector<DualMoonGraph::SceneObjectDrawData>& drawList)
{
    for (auto child : transform.GetChildren())
    {
        AppendDrawData(*child, drawList);
    }

    MeshRenderer* meshRenderer = transform.GetGameObject()->GetComponent<MeshRenderer>();
    if (meshRenderer)
    {
        auto mesh = meshRenderer->GetMesh();
        if (mesh == nullptr)
            return;

        auto& submeshes = mesh->submeshes;
        auto& materials = meshRenderer->GetMaterials();

        for (int i = 0; i < submeshes.size() || i < materials.size(); ++i)
        {
            auto material = i < materials.size() ? materials[i] : nullptr;
            auto submesh = i < submeshes.size() ? submeshes[i].get() : nullptr;
            auto shader = material ? material->GetShader() : nullptr;

            if (submesh && material && shader)
            {
                // material->SetMatrix("Transform", "model",
                // meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix());
                uint32_t indexCount = submesh->GetIndexCount();
                SceneObjectDrawData drawData;

                drawData.vertexBufferBinding = std::vector<Gfx::VertexBufferBinding>();
                for (auto& binding : submesh->GetBindings())
                {
                    drawData.vertexBufferBinding.push_back({submesh->GetVertexBuffer(), binding.byteOffset});
                }
                drawData.indexBuffer = submesh->GetIndexBuffer();
                drawData.indexBufferType = submesh->GetIndexBufferType();

                drawData.shaderResource = material->GetShaderResource().Get();
                drawData.shader = shader->GetShaderProgram().Get();
                drawData.shaderConfig = &material->GetShaderConfig();
                auto modelMatrix = meshRenderer->GetGameObject()->GetTransform()->GetModelMatrix();
                drawData.pushConstant = modelMatrix;
                drawData.indexCount = indexCount;
                drawList.push_back(drawData);
            }
        }
    }
}

void DualMoonGraph::Execute(Gfx::CommandBuffer& cmd)
{
    // culling here?
    auto& roots = scene.GetRootObjects();
    drawList.clear();
    for (auto root : roots)
    {
        AppendDrawData(*root->GetTransform(), drawList);
    }

    Graph::Execute(cmd);
}

} // namespace Engine
