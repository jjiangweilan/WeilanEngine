#include "GIScene.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "SurfelBakeFGNode.hpp"
#include "spdlog/spdlog.h"

namespace Engine::SurfelGI
{
void GISceneBaker::Bake(BakerConfig bakerConfig)
{
    // validation check
    if (bakerConfig.scene == nullptr || bakerConfig.worldBoundsMin == bakerConfig.worldBoundsMax ||
        bakerConfig.templateShader == nullptr)
        return;

    Scene* scene = bakerConfig.scene;
    auto cameraGO = scene->CreateGameObject();
    auto bakerCamera = cameraGO->AddComponent<Camera>();
    cameraGO->GetTransform()->SetPosition({0, 0, 0});
    cameraGO->GetTransform()->SetRotation({0, 0, 0});

    FrameGraph::Graph graph;
    graph.SetTemplateSceneShader(bakerConfig.templateShader);
    FrameGraph::SurfelBakeFGNode* surfelBakeNode = nullptr;
    FrameGraph::Node* sceneSort = nullptr;
    for (auto& bp : FrameGraph::NodeBlueprintRegisteration::GetNodeBlueprints())
    {
        if (bp.GetName() == "Surfel Bake Node")
        {
            surfelBakeNode = (FrameGraph::SurfelBakeFGNode*)&graph.AddNode(bp);
        }
        else if (bp.GetName() == "Scene Sort")
        {
            sceneSort = &graph.AddNode(bp);
        }
    }

    graph.Connect(sceneSort->GetOutput()[0].GetID(), surfelBakeNode->inputDrawList);
    graph.Compile();

    bakerCamera->SetFrameGraph(&graph);
    auto mainCam = scene->GetMainCamera();

    scene->SetMainCamera(bakerCamera);
    auto ortho = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.01f, 100.0f);
    ortho[1][1] *= -1;
    ortho[2] *= -1;
    bakerCamera->SetProjectionMatrix(ortho);

    auto albedoBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel albedo buf",
    });
    auto normalBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel normal buf",
    });
    auto positionBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel position buf",
    });

    ImmediateGfx::OnetimeSubmit(
        [&](Gfx::CommandBuffer& cmd)
        {
            auto graph = bakerCamera->GetFrameGraph();
            graph->Execute(cmd, *scene);

            Gfx::GPUBarrier layoutBarrier{
                .image = surfelBakeNode->albedo,
                .srcStageMask = Gfx::PipelineStage::Transfer,
                .dstStageMask = Gfx::PipelineStage::Transfer,
                .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .imageInfo = {
                    .oldLayout = Gfx::ImageLayout::Transfer_Dst,
                    .newLayout = Gfx::ImageLayout::Transfer_Src,
                    .subresourceRange = Gfx::ImageSubresourceRange{.baseMipLevel = surfelBakeNode->mipLevel - 1}}};
            cmd.Barrier(&layoutBarrier, 1);
            layoutBarrier.image = surfelBakeNode->normal;
            cmd.Barrier(&layoutBarrier, 1);
            layoutBarrier.image = surfelBakeNode->position;
            cmd.Barrier(&layoutBarrier, 1);

            Gfx::BufferImageCopyRegion region{
                .srcOffset = 0,
                .layers =
                    {
                        .aspectMask = Gfx::ImageAspectFlags::Color,
                        .mipLevel = surfelBakeNode->mipLevel - 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .offset = {0, 0, 0},
                .extend = {1, 1, 1},
            };
            std::vector<Gfx::BufferImageCopyRegion> copy{region};
            cmd.CopyImageToBuffer(surfelBakeNode->albedo, albedoBuf, copy);
            cmd.CopyImageToBuffer(surfelBakeNode->normal, normalBuf, copy);
            cmd.CopyImageToBuffer(surfelBakeNode->position, positionBuf, copy);
        }
    );

    glm::vec4 albedo{};
    glm::vec4 normal{};
    glm::vec4 position{};

    memcpy(&albedo, albedoBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&normal, normalBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&position, positionBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    SPDLOG_INFO("({}, {}, {})", albedo.x, albedo.y, albedo.z);

    scene->SetMainCamera(mainCam);
}

} // namespace Engine::SurfelGI
