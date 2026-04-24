#include "SHProbe.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Engine/Library/Image/LinearCubemap.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/ThirdParty/stb/stb_image_write.h"
#include <glm/gtc/random.hpp>
#include <spdlog/spdlog.h>
using namespace Rendering;
DEFINE_OBJECT(Object, SHProbe, "2D474098-4932-46EA-9911-C120F5595465");

SHProbe::SHProbe() {}
SHProbe::SHProbe(GameObject* gameObject) {}

void SHProbe::Init(int level) {}

std::array<float3, 9> SHProbe::BakeToSHCPU(
    Gfx::ImageDescription& cubeMapDesc, std::unique_ptr<Gfx::Buffer>& readbackBuffer
)
{
    LinearCubemap
        cubeMap(cubeMapDesc.width, cubeMapDesc.height, 4, sizeof(float), readbackBuffer->GetCPUVisibleAddress());

    struct Sample
    {
        float3 ray;
        float3 sample;
    };
    const int TotalSampleCount = 4096 * 10;
    std::vector<Sample> samples(TotalSampleCount);

    for (int i = 0; i < TotalSampleCount; ++i)
    {
        auto dir = glm::sphericalRand(1.0f);
        float3 color = cubeMap.Sample4<float>(dir);
        samples[i] = {dir, color};
    }

    std::array<float3, 9> sh = {};
    for (int l = 0; l <= 2; ++l)
    {
        for (int m = -l; m <= l; ++m)
        {
            for (int sampleIdx = 0; sampleIdx < TotalSampleCount; sampleIdx++)
            {
                int index = l * (l + 1) + m;
                float basis = SHBasis(l, m, samples[sampleIdx].ray);
                float3 color = samples[sampleIdx].sample;
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

    return sh;
}
void SHProbe::UpdateProbe(Scene& scene, const SHProbeUpdateSettings& settings)
{
    if (settings.skyboxOnly)
    {
        // render the skybox
        auto cmd = GetGfxDriver()->CreateCommandBuffer();
        Gfx::ImageDescription
            cubeMapDesc(256, 256, 1, Gfx::GfxFormat::R32G32B32A32_SFloat, Gfx::MultiSampling::Sample_Count_1, 1, true);

        auto cubeMapImage = GetGfxDriver()->CreateImage(
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

        auto cubeMapCam = scene.AddGameObject(std::make_unique<GameObject>());
        auto camera = cubeMapCam->AddComponent<Camera>();
        cubeMapCam->SetPosition(cubeMapCam->GetPosition());
        for (int face = 0; face < 6; ++face)
        {
            Gfx::ImageView::CreateInfo createInfo{
                .image = cubeMapImage.get(),
                .imageViewType = Gfx::ImageViewType::Image_2D,
                .subresourceRange =
                    Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, static_cast<uint32_t>(face), 1}
            };
            imageViews[face] = GetGfxDriver()->CreateImageView(createInfo);
            configs[face].colorOutputOverride = imageViews[face].get();
            configs[face].cmdOverride = cmd.get();

            camera->LookAt(cubeMapCam->GetPosition() + lookAtDirs[face]);
            skyboxRenderPipeline[face].SetConfig(configs[face]);
            skyboxRenderPipeline[face].RenderSkyboxOnly(scene, *camera, {});
        }

        Gfx::BufferImageCopyRegion copyRegion[] = {
            {0,
             Gfx::ImageSubresourceLayers{Gfx::ImageAspect::Color, 0, 0, 6},
             {0, 0, 0},
             {cubeMapDesc.width, cubeMapDesc.height, 1}}
        };
        cmd->CopyImageToBuffer(cubeMapImage, readbackBuffer, copyRegion);
        GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);
        cmd->Reset(true);
        scene.DestroyGameObject(cubeMapCam);

        // cpu baking to sh
        auto sh = BakeToSHCPU(cubeMapDesc, readbackBuffer);
        shData.clear();
        for (int i = 0; i < sh.size(); ++i)
        {
            shData.push_back(float4(sh[i], 1.0));
        }

        if (this->sh)
        {
            GetGfxDriver()->UploadBuffer(*this->sh, (uint8_t*)shData.data(), shData.size() * 4 * sizeof(float), 0);
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

void SHProbe::DebugDrawProbe(const float3& position)
{
    if (this->sh == nullptr)
    {
        this->sh = GetGfxDriver()->CreateBuffer(
            sizeof(float) * 4 * 9,
            Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Storage,
            false,
            false
        );

        GetGfxDriver()->UploadBuffer(*this->sh, (uint8_t*)shData.data(), shData.size() * 4 * sizeof(float), 0);
    }

    if (debugMaterial == nullptr)
    {
        debugMaterial = std::make_unique<Material>();
        // debugMaterial->SetShader(ShaderLibrary::GetShader(Shaders::SHProbe)); TODO: Add SHProbe to Shaders
        debugMaterial->GetShaderResource()->SetBuffer("sh", sh.get());
    }

    auto sphere = EngineInternalResources::GetModels().sphere;
    auto model = glm::translate(float4x4(1.0), position);
    Graphics::DrawMesh(*sphere, 0, model, *debugMaterial);
}
