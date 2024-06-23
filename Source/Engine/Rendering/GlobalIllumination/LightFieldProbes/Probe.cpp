#include "Probe.hpp"
#include "Asset/Material.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Rendering::LFP
{
Probe::Probe(const glm::vec3& pos) : position(pos)
{
    albedo = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            octahedralMapSize,
            octahedralMapSize,
            1,
            Gfx::ImageFormat::R8G8B8A8_SRGB,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            false
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );

    normal = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            octahedralMapSize,
            octahedralMapSize,
            1,
            Gfx::ImageFormat::A2B10G10R10_UNorm,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            false
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );

    radialDistance = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            octahedralMapSize,
            octahedralMapSize,
            1,
            Gfx::ImageFormat::R32_Float,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            false
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );
}

static Shader* GetLightFieldProbePreviewShader()
{
    static Shader* s;
    if (s == nullptr)
    {
        s = (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/LightFieldProbePreview.shad");
    }
    return s;
}

Material* Probe::GetPreviewMaterial()
{
    if (material == nullptr)
    {
        material = std::make_unique<Material>(GetLightFieldProbePreviewShader());
        material->SetTexture("albedoTex", albedo.get());
        material->SetTexture("normalTex", normal.get());
        material->SetTexture("radialDistanceTex", radialDistance.get());
    }

    return material.get();
}
} // namespace Rendering::LFP
