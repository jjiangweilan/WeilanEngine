#include "SHProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderPipeline/RenderPipeline.hpp"

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
            cubeMapDesc(256, 256, 1, Gfx::GfxFormat::R16G16B16A16_SFloat, Gfx::MultiSampling::Sample_Count_1, 1, true);

        cubeMap = GetGfxDriver()->CreateImage(
            cubeMapDesc,
            Gfx::ImageUsage::Storage | Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
        );
        std::unique_ptr<Gfx::ImageView> imageViews[6];
        RenderPipeline skyboxRenderPipeline[6];
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

        GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);
        cmd->Reset(true);
        scene->DestroyGameObject(cubeMapCam);
    }
    else
    {
        spdlog::warn("other sh probe not implemented");
    }
}
