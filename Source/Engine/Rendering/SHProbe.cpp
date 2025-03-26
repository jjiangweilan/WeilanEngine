#include "SHProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Libs/Image/LinearCubemap.hpp"
#include "Rendering/RenderPipeline/RenderPipeline.hpp"
#include "ThirdParty/stb/stb_image_write.h"
#include <glm/gtc/random.hpp>
#include <spdlog/spdlog.h>
using namespace Rendering;
DEFINE_OBJECT(SHProbe, "2D474098-4932-46EA-9911-C120F5595465");

SHProbe::SHProbe() : Component(nullptr) {}
SHProbe::SHProbe(GameObject* gameObject) : Component(gameObject) {}
const std::string& SHProbe::GetName()
{
    static std::string name = "SHProbe";
    return name;
}

void SHProbe::Init(int level) {}

void SHProbe::UpdateProbe(const SHProbeUpdateSettings& settings)
{
    auto scene = GetScene();
    ASSERT(scene != nullptr);

    if (settings.skyboxOnly)
    {
        // render the skybox
        auto cmd = GetGfxDriver()->CreateCommandBuffer();
        Gfx::ImageDescription
            cubeMapDesc(256, 256, 1, Gfx::GfxFormat::R32G32B32A32_SFloat, Gfx::MultiSampling::Sample_Count_1, 1, true);

        cubeMap = GetGfxDriver()->CreateImage(
            cubeMapDesc,
            Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment |
                Gfx::ImageUsage::TransferSrc
        );

        Gfx::Buffer::CreateInfo staingBufferCreateInfo{
            Gfx::BufferUsage::Transfer_Dst,
            cubeMapDesc.GetByteSize(),
            true,
            "SH Probe Readback Buffer",
            true
        };
        auto readbackBuffer = GetGfxDriver()->CreateBuffer(staingBufferCreateInfo);
        RenderPipeline skyboxRenderPipeline[6];
        std::unique_ptr<Gfx::ImageView> imageViews[6];
        Rendering::RenderConfig configs[6];
        float3 lookAtDirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

        auto cubeMapCam = scene->AddGameObject(std::make_unique<GameObject>());
        auto camera = cubeMapCam->AddComponent<Camera>();
        cubeMapCam->SetPosition(GetGameObject()->GetPosition());
        for (int face = 0; face < 6; ++face)
        {
            Gfx::ImageView::CreateInfo createInfo{
                .image = *cubeMap.get(),
                .imageViewType = Gfx::ImageViewType::Image_2D,
                .subresourceRange =
                    Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, static_cast<uint32_t>(face), 1}
            };
            imageViews[face] = GetGfxDriver()->CreateImageView(createInfo);
            configs[face].colorOutputOverride = imageViews[face].get();
            configs[face].cmdOverride = cmd.get();

            camera->LookAt(cubeMapCam->GetPosition() + lookAtDirs[face]);
            skyboxRenderPipeline[face].SetConfig(configs[face]);
            skyboxRenderPipeline[face].RenderSkyboxOnly(*scene, *camera, {});
        }

        Gfx::BufferImageCopyRegion copyRegion[] = {
            {0,
             Gfx::ImageSubresourceLayers{Gfx::ImageAspect::Color, 0, 0, 6},
             {0, 0, 0},
             {cubeMapDesc.width, cubeMapDesc.height, 1}}
        };
        cmd->CopyImageToBuffer(cubeMap, readbackBuffer, copyRegion);
        GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);
        cmd->Reset(true);
        scene->DestroyGameObject(cubeMapCam);

        // cpu baking to sh
        {
            LinearCubemap cubeMap(
                cubeMapDesc.width,
                cubeMapDesc.height,
                4,
                sizeof(float),
                readbackBuffer->GetCPUVisibleAddress()
            );

            float3 sh[9]{
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0},
                {0, 0, 0}
            };

            const int TotalSampleCount = 1024;
            for (int l = 0; l <= 2; ++l)
            {
                for (int m = -l; m <= l; ++m)
                {
                    for (int sampleIdx = 0; sampleIdx < TotalSampleCount; sampleIdx++)
                    {
                        int index = l * (l + 1) + m;
                        auto dir = glm::sphericalRand(1.0f);
                        float basis = SHBasis(l, m, dir);
                        float3 color = cubeMap.Sample4<float>(dir);
                        sh[index] += color * basis;
                    }
                }
            }

            float weight = 4.0f * glm::pi<float>() / TotalSampleCount;
            for (int i = 0; i < 9; ++i)
            {
                sh[i] *= weight;
                spdlog::info("{}, {}, {}", sh[i].x, sh[i].y, sh[i].z);
            }
        }
    }
    else
    {
        spdlog::warn("other sh probe not implemented");
    }
}

float SHProbe::SHBasis(int l, int m, float3 dir)
{
    if (l == 0 && m == 0)
    {
        return 0.282095; // Y00
    }
    else if (l == 1 && m == -1)
    {
        return 0.488603 * dir.y; // Y1-1
    }
    else if (l == 1 && m == 0)
    {
        return 0.488603 * dir.z; // Y10
    }
    else if (l == 1 && m == 1)
    {
        return 0.488603 * dir.x; // Y11
    }
    else if (l == 2 && m == -2)
    {
        return 1.092548 * dir.x * dir.y; // Y2-2
    }
    else if (l == 2 && m == -1)
    {
        return 1.092548 * dir.y * dir.z; // Y2-1
    }
    else if (l == 2 && m == 0)
    {
        return 0.315392 * (3.0 * dir.z * dir.z - 1.0); // Y20
    }
    else if (l == 2 && m == 1)
    {
        return 1.092548 * dir.x * dir.z; // Y21
    }
    else if (l == 2 && m == 2)
    {
        return 0.546274 * (dir.x * dir.x - dir.y * dir.y); // Y22
    }
    return 0.0;
}
