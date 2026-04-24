#include "Probe.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "ProbeBaker.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

namespace Rendering::LFP
{
Probe::Probe(const glm::vec3& pos) : position(pos)
{
    albedo = GetGfxDriver()->CreateImage(
        Gfx::ImageDescription{
            octahedralMapSize,
            octahedralMapSize,
            1,
            Gfx::GfxFormat::R8G8B8A8_SRGB,
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
            Gfx::GfxFormat::A2B10G10R10_UNorm,
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
            Gfx::GfxFormat::R32_SFloat,
            Gfx::MultiSampling::Sample_Count_1,
            1,
            false
        },
        Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
    );
}
Probe::~Probe(){};

void Probe::Bake(Gfx::CommandBuffer& cmd, DrawList* drawList, ProbeBaker& baker)
{
    baker.Bake(cmd, drawList);
}
} // namespace Rendering::LFP
