#include "GIScene.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "SurfelBakeFGNode.hpp"
#include "spdlog/spdlog.h"

namespace Engine::SurfelGI
{
void GISceneBaker::Bake(BakerConfig bakerConfig)
{
    // validation check
    if (bakerConfig.scene == nullptr || bakerConfig.worldBoundsMin == bakerConfig.worldBoundsMax || bakerConfig.templateShader == nullptr)
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
    auto ortho = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.01f, 1.0f);
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
        .debugName = "surfel albedo buf",
    });
    auto positionBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel albedo buf",
    });

    ImmediateGfx::OnetimeSubmit(
        [&](Gfx::CommandBuffer& cmd)
        {
            auto graph = bakerCamera->GetFrameGraph();
            graph->Execute(cmd, *scene);

            Gfx::GPUBarrier barrier{
                .srcStageMask = Gfx::PipelineStage::Bottom_Of_Pipe,
                .dstStageMask = Gfx::PipelineStage::Top_Of_Pipe,
                .srcAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
                .dstAccessMask = Gfx::AccessMask::Memory_Read | Gfx::AccessMask::Memory_Write,
            };
            cmd.Barrier(&barrier, 1);

            Gfx::BufferImageCopyRegion region{
                .srcOffset = 0,
                .layers =
                    {
                        .aspectMask = Gfx::ImageAspectFlags::Color,
                        .mipLevel = surfelBakeNode->mipLevel,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .offset = {0, 0, 0},
                .extend = {1, 1, 1},
            };
            std::vector<Gfx::BufferImageCopyRegion> copy{region};
            cmd.CopyImageToBuffer(surfelBakeNode->position, albedoBuf, copy);
            cmd.CopyImageToBuffer(surfelBakeNode->normal, normalBuf, copy);
            cmd.CopyImageToBuffer(surfelBakeNode->position, positionBuf, copy);
        }
    );

    glm::vec4 albedo;
    glm::vec4 normal;
    glm::vec4 position;

    memcpy(&albedo, albedoBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&normal, normalBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&position, positionBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    SPDLOG_INFO("({}, {}, {})", albedo.x, albedo.y, albedo.z);

    scene->SetMainCamera(mainCam);
}

} // namespace Engine::SurfelGI
