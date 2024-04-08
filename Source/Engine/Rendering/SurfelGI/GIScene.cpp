#include "GIScene.hpp"
#include "spdlog/spdlog.h"

namespace SurfelGI
{
DEFINE_ASSET(GIScene, "E59E4458-150B-4FE0-B45C-BB5676EDB7F8", "giscene")

GIScene GISceneBaker::Bake(BakerConfig bakerConfig)
{
    // validation check
    if (bakerConfig.scene == nullptr || bakerConfig.worldBoundsMin == bakerConfig.worldBoundsMax ||
        bakerConfig.templateShader == nullptr)
        return GIScene();

    scene = bakerConfig.scene;
    cameraGO = scene->CreateGameObject();
    bakerCamera = cameraGO->AddComponent<Camera>();
    cameraGO->SetPosition({0, 0, 0});
    cameraGO->SetRotation({0, 0, 0});

    FrameGraph::Graph graph;
    surfelBakeNode = nullptr;
    FrameGraph::Node* sceneSort = nullptr;
    for (auto& bp : FrameGraph::NodeBlueprintRegisteration::GetNodeBlueprints())
    {
        //if (bp.GetName() == "Surfel Bake Node")
        //{
        //    surfelBakeNode = (FrameGraph::SurfelBakeFGNode*)&graph.AddNode(bp);
        //}
        //else if (bp.GetName() == "Scene Sort")
        //{
        //    sceneSort = &graph.AddNode(bp);
        //}
    }

    graph.Connect(sceneSort->GetOutput()[0].GetID(), surfelBakeNode->inputDrawList);
    graph.Compile();

    bakerCamera->SetFrameGraph(&graph);
    auto mainCam = scene->GetMainCamera();

    scene->SetMainCamera(bakerCamera);
    float boxSize = bakerConfig.surfelBoxSize;
    float halfBoxSize = boxSize / 2.0f;
    auto ortho = glm::ortho(-halfBoxSize, halfBoxSize, -halfBoxSize, halfBoxSize, 0.0f, boxSize);
    ortho[1][1] *= -1;
    ortho[2] *= -1;
    bakerCamera->SetProjectionMatrix(ortho);

    albedoBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel albedo buf",
    });
    positionBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel position buf",
    });
    normalBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
        .usages = Gfx::BufferUsage::Transfer_Dst,
        .size = Gfx::MapImageFormatToByteSize(Gfx::ImageFormat::R32G32B32A32_SFloat),
        .visibleInCPU = true,
        .debugName = "surfel normal buf",
    });

    auto bmin = bakerConfig.worldBoundsMin;
    auto bmax = bakerConfig.worldBoundsMax;

    GIScene giScene;
    for (int i = bmin.x; i < bmax.x; i += boxSize)
    {
        for (int j = bmin.y; j <= bmax.y; j += boxSize)
        {
            for (int k = bmin.z; k <= bmax.z; k += boxSize)
            {
                for (int nIndex = 0; nIndex < principleNormals.size(); ++nIndex)
                {
                    glm::vec3 principleNormal = principleNormals[nIndex];
                    glm::vec3 center{i + halfBoxSize, j + halfBoxSize, k + halfBoxSize};
                    glm::vec3 pos = center + principleNormal * halfBoxSize;
                    glm::mat4 model(1);
                    if (nIndex == 0)
                        model =
                            glm::rotate(glm::translate(glm::mat4(1), pos), glm::radians(-90.0f), glm::vec3(0, 1, 0));
                    else if (nIndex == 1)
                        model = glm::rotate(glm::translate(glm::mat4(1), pos), glm::radians(90.0f), glm::vec3(0, 1, 0));
                    else if (nIndex == 2)
                        model = glm::rotate(glm::translate(glm::mat4(1), pos), glm::radians(90.0f), glm::vec3(1, 0, 0));
                    else if (nIndex == 3)
                        model =
                            glm::rotate(glm::translate(glm::mat4(1), pos), glm::radians(-90.0f), glm::vec3(1, 0, 0));
                    else if (nIndex == 4)
                        model = glm::translate(glm::mat4(1), pos);
                    else if (nIndex == 5)
                        model =
                            glm::rotate(glm::translate(glm::mat4(1), pos), glm::radians(180.0f), glm::vec3(1, 0, 0));

                    giScene.surfels.push_back(CaptureSurfel(model, halfBoxSize));
                }
            }
        }
    }

    scene->SetMainCamera(mainCam);
    giScene.SetName("GI Scene");
    return giScene;
}

Surfel GISceneBaker::CaptureSurfel(const glm::mat4& camModel, float halfBoxSize)
{
    cameraGO->SetModelMatrix(camModel);

    GetGfxDriver()->ExecuteImmediately(
        [&](Gfx::CommandBuffer& cmd)
        {
            auto graph = bakerCamera->GetFrameGraph();
            graph->Execute(cmd, *scene);

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
            cmd.CopyImageToBuffer(surfelBakeNode->position, positionBuf, copy);
            cmd.CopyImageToBuffer(surfelBakeNode->normal, normalBuf, copy);
        }
    );

    glm::vec4 albedo{};
    glm::vec4 position{};
    glm::vec4 normal{};

    memcpy(&albedo, albedoBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&position, positionBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));
    memcpy(&normal, normalBuf->GetCPUVisibleAddress(), sizeof(glm::vec4));

    if (glm::any(glm::isnan(albedo)))
    {
        albedo = glm::vec4(0, 0, 0, 0);
        SPDLOG_WARN("GI Baker: nan albedo detected, converting all to zero");
    }
    if (glm::any(glm::isnan(position)))
    {
        position = glm::vec4(0, 0, 0, 0);
        SPDLOG_WARN("GI Baker: nan position detected, converting all to zero");
    }
    if (glm::any(glm::isnan(normal)))
    {
        normal = glm::vec4(0, 0, 0, 0);
        SPDLOG_WARN("GI Baker: nan normal detected, converting all to zero");
    }

    return Surfel(albedo, position, normal);
}

void GIScene::Serialize(Serializer* s) const
{
    Asset::Serialize(s);

    s->Serialize("surfels", surfels);
}
void GIScene::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);

    s->Deserialize("surfels", surfels);
}

void Surfel::Serialize(Serializer* s) const
{
    s->Serialize("albedo", albedo);
    s->Serialize("position", position);
    s->Serialize("normal", normal);
}
void Surfel::Deserialize(Serializer* s)
{
    s->Deserialize("albedo", albedo);
    s->Deserialize("position", position);
    s->Deserialize("normal", normal);
}
void GIScene::Reload(Asset&& asset)
{
    if (GIScene* newGIScene = dynamic_cast<GIScene*>(&asset))
    {
        this->surfels = std::move(newGIScene->surfels);
        newGIScene->surfels.clear();
    }
}
} // namespace SurfelGI
