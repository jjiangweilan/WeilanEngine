#pragma once
#include "GfxDriver/GfxDriver.hpp"
class Material;
namespace Rendering::LFP
{
class Probe
{
public:
    Probe(const glm::vec3& pos);

    glm::vec3 GetPosition()
    {
        return position;
    }

    Gfx::Image* GetAlbedo()
    {
        return albedo.get();
    }
    Gfx::Image* GetNormal()
    {
        return normal.get();
    }
    Gfx::Image* GetRadialDistance()
    {
        return radialDistance.get();
    }

    Material* GetPreviewMaterial();

private:
    glm::vec3 position;

    // octahedral maps
    std::unique_ptr<Gfx::Image> albedo;
    std::unique_ptr<Gfx::Image> normal;
    std::unique_ptr<Gfx::Image> radialDistance;
    std::unique_ptr<Material> material;

    const uint32_t octahedralMapSize = 256;

    friend class ProbeBaker;
};
} // namespace Rendering::LFP
