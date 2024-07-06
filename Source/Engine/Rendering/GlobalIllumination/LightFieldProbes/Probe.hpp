#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/DrawList.hpp"
class Material;
namespace Rendering::LFP
{
class ProbeBaker;
class Probe
{
public:
    Probe(const glm::vec3& pos);
    Probe(Probe&& pos) = default;
    ~Probe();

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

    void Bake(Gfx::CommandBuffer& cmd, DrawList* drawList);

    bool IsBaked() const
    {
        return baked;
    }

    ProbeBaker* GetBaker()
    {
        return baker.get();
    }

private:
    glm::vec3 position;

    // octahedral maps
    std::unique_ptr<Gfx::Image> albedo;
    std::unique_ptr<Gfx::Image> normal;
    std::unique_ptr<Gfx::Image> radialDistance;
    std::unique_ptr<Material> material;

    static const uint32_t octahedralMapSize = 256;

    std::unique_ptr<ProbeBaker> baker;

    // set by probe baker
    bool baked = false;

    friend ProbeBaker;
};
} // namespace Rendering::LFP
