#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Probe.hpp"
#include <vector>
class Scene;
namespace Rendering::LFP
{
class LightFieldProbes
{
public:
    // place probes in grid vertices
    void PlaceProbes(const glm::vec3& basePosition, const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count);
    Probe* GetProbe(glm::vec3 index)
    {
        int i = index.z * probeCount.y * probeCount.x + index.y * probeCount.x + index.x;
        if (i >= 0 && i < probes.size())
        {
            return &probes[i];
        }
        return nullptr;
    };
    void BakeProbeGBuffers(Scene* scene);

    std::span<Probe> GetProbes()
    {
        return probes;
    }

private:
    std::vector<Probe> probes;
    glm::vec3 probeCount;
};
} // namespace Rendering::LFP
