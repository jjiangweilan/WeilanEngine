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

    void Relight(Gfx::CommandBuffer& cmd);
    void Bake(Gfx::CommandBuffer& cmd, DrawList* drawList, ProbeBaker& baker);

    bool IsBaked() const
    {
        return baked;
    }

private:
    glm::vec3 position;

    // octahedral maps
    std::unique_ptr<Gfx::Image> albedo;
    std::unique_ptr<Gfx::Image> normal;
    std::unique_ptr<Gfx::Image> radialDistance;

    std::unique_ptr<Gfx::Image> lighted;

    static const uint32_t octahedralMapSize = 128;

    // set by probe baker
    bool baked = false;

    friend ProbeBaker;
};
} // namespace Rendering::LFP
